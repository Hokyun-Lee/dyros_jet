#include "dyros_jet_controller/task_controller.h"
#include "dyros_jet_controller/dyros_jet_model.h"

namespace dyros_jet_controller
{

constexpr unsigned int TaskController::PRIORITY;

void TaskController::compute()
{
  // update state
  computeCLIK();
  //
}
void TaskController::setTarget(DyrosJetModel::EndEffector ee, Eigen::Isometry3d target, double start_time, double end_time)
{
  start_transform_[ee] = model_.getCurrentTrasmfrom(ee);
  target_transform_[ee] = target;
  start_time_[ee] = start_time;
  end_time_[ee] = end_time;
  x_prev_[ee] = start_transform_[ee].translation();
}
void TaskController::setTarget(DyrosJetModel::EndEffector ee, Eigen::Isometry3d target, double duration)
{
  setTarget(ee, target, control_time_, control_time_ + duration);
}
void TaskController::setEnable(DyrosJetModel::EndEffector ee, bool enable)
{
  ee_enabled_[ee] = enable;
}
void TaskController::updateControlMask(unsigned int *mask)
{
  unsigned int index = 0;
  for(int i=0; i<total_dof_; i++)
  {
    if(i < 6)
    {
      index = 0;
    }
    else if (i < 6 + 6)
    {
      index = 1;
    }
    else if (i < 6 + 6 + 2)
    {
      continue; // waist
    }
    else if (i < 6 + 6 + 2 + 7)
    {
      index = 2;
    }
    else if (i < 6 + 6 + 2 + 7 + 7)
    {
      index = 3;
    }

    if(ee_enabled_[index])
    {
      if (mask[i] >= PRIORITY * 2)
      {
        // Higher priority task detected
        setTarget((DyrosJetModel::EndEffector)index, model_.getCurrentTrasmfrom((DyrosJetModel::EndEffector)index), 0); // Stop moving
      }
      mask[i] = (mask[i] | PRIORITY);
    }
    else
    {
      mask[i] = (mask[i] & ~PRIORITY);
      setTarget((DyrosJetModel::EndEffector)index, model_.getCurrentTrasmfrom((DyrosJetModel::EndEffector)index), 0); // Stop moving
    }
  }
}


void TaskController::writeDesired(const unsigned int *mask, VectorQd& desired_q)
{
  for(unsigned int i=0; i<total_dof_; i++)
  {
    if( mask[i] >= PRIORITY && mask[i] < PRIORITY * 2 )
    {
      desired_q(i) = desired_q_(i);
    }
  }
}

// Jacobian OK. Translation OK.
void TaskController::computeCLIK()
{

  const double inverse_damping = 0.03;
  const double phi_gain = 1.0;
  const double kp = 200;

  // Arm
  for(unsigned int i=0; i<4; i++)
  {
    if(ee_enabled_[i])
    {
      // For short names
      const auto &x_0 = start_transform_[i].translation();
      // const auto &rot_0 = start_transform_[i].linear();

      const auto &x = model_.getCurrentTrasmfrom((DyrosJetModel::EndEffector)(i)).translation();
      const auto &rot = model_.getCurrentTrasmfrom((DyrosJetModel::EndEffector)(i)).linear();

      const auto &x_target = target_transform_[i].translation();
      const auto &rot_target = target_transform_[i].linear();

      double h_cubic = DyrosMath::cubic(control_time_,
                                        start_time_[i],
                                        end_time_[i],
                                        0, 1, 0, 0);

      Eigen::Vector3d x_cubic;
      Eigen::Vector6d x_dot_desired;
      x_cubic = DyrosMath::cubicVector<3>(control_time_,
                                          start_time_[i],
                                          end_time_[i],
                                          x_0,
                                          x_target,
                                          Eigen::Vector3d::Zero(),//start_x_dot_[i],
                                          Eigen::Vector3d::Zero());
      Eigen::Vector6d x_error;
      x_error.head<3>() = (x_cubic - x);
      //x_error.tail<3>().setZero();
      h_cubic = 1.0;
      x_error.tail<3>() =  - h_cubic * phi_gain * DyrosMath::getPhi(rot, rot_target);
      x_dot_desired.head<3>() = x_cubic - x_prev_[i];
      x_dot_desired.tail<3>().setZero();

      x_error = x_error; //* hz_;
      x_dot_desired = x_dot_desired * hz_;
      x_prev_[i] = x_cubic;

      if (i < 2)  // Legs
      {
        const auto &J = model_.getLegJacobian((DyrosJetModel::EndEffector)(i));
        const auto &q = current_q_.segment<6>(model_.joint_start_index_[i]);

        auto J_inverse = J.transpose() *
            (inverse_damping * Eigen::Matrix6d::Identity() +
             J * J.transpose()).inverse();

        desired_q_.segment<6>(model_.joint_start_index_[i])
            = (J_inverse * (x_dot_desired + x_error * kp)) / hz_ + q;
      }
      else    // Arms
      {
        const auto &J = model_.getArmJacobian((DyrosJetModel::EndEffector)(i));
        const auto &q = current_q_.segment<7>(model_.joint_start_index_[i]);


        auto J_inverse = J.transpose() *
            (inverse_damping * Eigen::Matrix6d::Identity() +
             J * J.transpose()).inverse();


        desired_q_.segment<7>(model_.joint_start_index_[i]) =
            (J_inverse * (x_dot_desired + x_error * kp)) / hz_ + q;


        std::cout << "Jacobian : " << std::endl;
        std::cout << J << std::endl;
        std::cout << "Jacobian.inv : "<< std::endl;
        std::cout << J_inverse << std::endl;
        std::cout << "desired_q_ : " << std::endl << desired_q_.segment<7>(model_.joint_start_index_[i]) << std::endl;
        std::cout << "x : " << std::endl << x << std::endl;
        std::cout << "q : " << std::endl << current_q_ << std::endl;
        std::cout << "rot : " << std::endl << rot << std::endl;
        std::cout << "x_cubic : " << std::endl << x_cubic << std::endl;
        std::cout << "x_error : " << std::endl << x_error << std::endl;
        std::cout << "x_dot_desired : " << std::endl << x_dot_desired << std::endl;

      }
    }
  }
}

}
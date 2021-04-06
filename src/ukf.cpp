#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
 */

UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);
  // Initialize state covariance matrix
  P_.setIdentity();
  P_(0, 0) = 0.04;
  P_(1, 1) = 0.04;

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 0.75;//30;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = (17.25/180.)* M_PI;//30;

  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.

  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */
  is_initialized_ = false;
  n_x_ = 5;
  n_aug_ = 7;
  lambda_ = 3 - n_aug_;
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);
  Xsig_aug_ = MatrixXd(n_aug_, 2 * n_aug_ + 1);

  // Initialize the weights
  weights_ = VectorXd(2*n_aug_+1);
  int n_sigmapoints = 2*n_aug_+1;
  weights_(0) = lambda_ / (lambda_ + n_aug_);
  weights_.tail(n_sigmapoints-1).fill(weights_(0)*(0.5/lambda_));

  nis_values_radar_file.open("NIS Values_Radar.csv");
  nis_values_laser_file.open("NIS Values_Laser.csv");
}

UKF::~UKF() {
  nis_values_radar_file.close();
  cout << "Closed radar nis file.";

  nis_values_laser_file.close();
  cout << "Closed laser nis file.";
}

/**
* Generate Sigma points around the current state under consideration of the
* current state uncertainty.
**/
void UKF::GenerateSigmaPoints(){
  //create sigma point matrix
  MatrixXd Xsig = MatrixXd(n_x_, 2 * n_x_ + 1);

  //calculate square root of P
  MatrixXd A = P_.llt().matrixL();

  Xsig.col(0) = x_;

  for(int i = 1; i<=n_x_; i++){
    Xsig.col(i) = x_ + sqrt(lambda_ + n_x_) * A.col(i-1);
    Xsig.col(i+n_x_) = x_ - sqrt(lambda_ + n_x_) * A.col(i-1);
  }

  // Pass matrix by value
  Xsig_ = Xsig;

}

/**
* Augment the mean state to include the process noise parameters.
**/
void UKF::AugmentedSigmaPoints(){

  //create augmented mean vector
  VectorXd x_aug = VectorXd(n_aug_);

  //create augmented state covariance
  MatrixXd P_aug = MatrixXd(n_aug_, 7);

  //create sigma point matrix
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);

  //create augmented mean stateClosed both files
  x_aug.head(n_x_) = x_;
  x_aug(n_x_) = 0.;
  x_aug(n_x_+1) = 0.;

  //create augmented covariance matrix
  P_aug.fill(0.0);
  P_aug.topLeftCorner(n_x_,n_x_) = P_;
  P_aug(n_x_,n_x_) = std_a_*std_a_;
  P_aug(n_x_+1,n_x_+1) = std_yawdd_*std_yawdd_;

  //create square root matrix
  MatrixXd L = P_aug.llt().matrixL();

  //create augmented sigma points
  Xsig_aug.col(0)  = x_aug;
  for (int i = 0; i< n_aug_; i++)
  {
    Xsig_aug.col(i+1)       = x_aug + sqrt(lambda_+n_aug_) * L.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_+n_aug_) * L.col(i);
  }

  //print result
  std::cout << "Xsig_aug = " << std::endl << Xsig_aug << std::endl;

  //write result
  Xsig_aug_ = Xsig_aug;
}

/**
* This method implements the prediction of the augmented state represented
* in sigma points. The result are the expected sigma points that follow from
* the process model. Taken from the lecture quiz.
**/
void UKF::SigmaPointPrediction(float delta_t){

    //predict sigma points
    for (int i = 0; i< 2*n_aug_+1; i++)
    {
      //extract values for better readability
      double p_x = Xsig_aug_(0,i);
      double p_y = Xsig_aug_(1,i);
      double v = Xsig_aug_(2,i);
      double yaw = Xsig_aug_(3,i);
      double yawd = Xsig_aug_(4,i);
      double nu_a = Xsig_aug_(5,i);
      double nu_yawdd = Xsig_aug_(6,i);

      //predicted state values
      double px_p, py_p;

      //avoid division by zero
      if (fabs(yawd) > 0.001) {
          px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
          py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
      }
      else {
          px_p = p_x + v*delta_t*cos(yaw);
          py_p = p_y + v*delta_t*sin(yaw);
      }

      double v_p = v;
      double yaw_p = yaw + yawd*delta_t;
      double yawd_p = yawd;

      //add noise
      px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
      py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
      v_p = v_p + nu_a*delta_t;

      yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
      yawd_p = yawd_p + nu_yawdd*delta_t;

      //write predicted sigma point into right column
      Xsig_pred_(0,i) = px_p;
      Xsig_pred_(1,i) = py_p;
      Xsig_pred_(2,i) = v_p;
      Xsig_pred_(3,i) = yaw_p;
      Xsig_pred_(4,i) = yawd_p;
    }

    //print result
    std::cout << "Xsig_pred_ = " << std::endl << Xsig_pred_ << std::endl;

}

void UKF::PredictMeanAndCovariance() {

  int n_sigmapoints = 2*n_aug_+1;

  //predict state mean
  x_ = weights_(0) * Xsig_pred_.col(0);
  //std::cout << "x_ at first" << x_ << std::endl;

  for(int i=1;i<n_sigmapoints; ++i){
      x_ += weights_(i) * Xsig_pred_.col(i);
  }
  //std::cout <<"X total" <<x_ << std::endl;

  //predict state covariance matrix
  VectorXd diff = Xsig_pred_.col(0) - x_;
  P_ = weights_(0) * diff*diff.transpose();
  for(int i=1;i<n_sigmapoints; ++i){
      diff = Xsig_pred_.col(i) - x_;
      P_ += weights_(i) * diff*diff.transpose();
  }

}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */

  /*****************************************************************************
   *  Initialization
   ****************************************************************************/
  if (!is_initialized_) {
    /**
    Done:
      * Initialize the state ekf_.x_ with the first measurement.
      * Create the covariance matrix.
      * Remember: you'll need to convert radar from polar to cartesian coordinates.
    */
    // first measurement
    cout << "UKF: " << endl;
    //ekf_.x_ = VectorXd(4);
    //ekf_.x_ << 1, 1, 1, 1;

    if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
      /**
      Convert radar from polar to cartesian coordinates and initialize state.
      */
      cout << "Radar measurement first" << endl;

      float rho = meas_package.raw_measurements_(0);
      float phi = meas_package.raw_measurements_(1);

      float x_p = rho * cos(phi);
      float y_p = rho * sin(phi);

      // Not correct, but gives an estimate
      float v = 5.;
      float psi = 1;
      float psi_dot = 0.1;

      cout << "Radar init: "<< meas_package.raw_measurements_ << endl;
      previous_timestamp_ = meas_package.timestamp_;

      x_ << x_p, y_p, v, psi, psi_dot;
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
      /**
      Initialize state.
      */
      cout << "Lidar measurement first" << endl;

      float x_p = meas_package.raw_measurements_(0);
      float y_p = meas_package.raw_measurements_(1);

      // Not correct, but gives an estimate
      float v = 5.;
      float psi = 1;
      float psi_dot = 0.1;

      cout << "Obtained first measurements." << endl;
      x_ << x_p, y_p, v, psi, psi_dot;
      cout << "Stored X." << endl;
      previous_timestamp_ = meas_package.timestamp_;

    }

    // done initializing, no need to predict or update
    is_initialized_ = true;
    cout << "Init after first measurement done" << endl;
    return;
  }

  /*****************************************************************************
   *  Prediction
   ****************************************************************************/

  /**
   Done:
     * Update the state transition matrix F according to the new elapsed time.
      - Time is measured in seconds.
     * Update the process noise covariance matrix.
     * Use noise_ax = 9 and noise_ay = 9 for your Q matrix.
   */

   cout << "Preparing prediction" << endl;

  float dt = (meas_package.timestamp_ - previous_timestamp_)/1000000.0;
  previous_timestamp_ = meas_package.timestamp_;

  cout << "Predict..." << endl;

  Prediction(dt);

  cout << "... done." << endl;

  /*****************************************************************************
   *  Update
   ****************************************************************************/

  /**
   Done:
     * Use the sensor type to perform the update step.
     * Update the state and covariance matrices.
   */

  if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {

    UpdateRadar(meas_package);

  } else {
    // Laser updates
    cout << "Laser measurement update" << endl;
    UpdateLidar(meas_package);

    }

}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */
  MatrixXd Xsig = MatrixXd(n_x_, 2 * n_x_ + 1);

  // Generate Sigma Points
  GenerateSigmaPoints();

  // Augment the SigmaPoints by the Process Noise parameters
  AugmentedSigmaPoints();

  // Predict Sigma Points
  SigmaPointPrediction(delta_t);

  // Predict Mean and Covariance
  PredictMeanAndCovariance();
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */

  double px, py, px_meas, py_meas;

  px_meas = meas_package.raw_measurements_(0);
  py_meas = meas_package.raw_measurements_(1);

  unsigned n_z = 2;
  MatrixXd S = MatrixXd(n_z,n_z);

  VectorXd z = VectorXd(n_z);
  z << px_meas, py_meas;

  //create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_, n_z);

  //create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);

  //transform sigma points into measurement space
  for(int col=0;col<(2*n_aug_+1);col++){
      px = Xsig_pred_(0,col);
      py = Xsig_pred_(1,col);

      Zsig(0,col) = px;
      Zsig(1,col) = py;
  }

  //calculate mean predicted measurement
  z_pred.fill(0.0);
  for (int col=0; col < 2*n_aug_+1; col++) {
      z_pred = z_pred + weights_(col) * Zsig.col(col);
  }


  //calculate innovation covariance matrix S
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  //add measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_z,n_z);
  R <<    std_laspx_*std_laspx_, 0,
          0, std_laspy_*std_laspy_;
  S = S + R;

  //calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  //Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  //residual
  VectorXd z_diff = z - z_pred;

  //update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();

  // Calculate NIS
  float nis_epsilon = z_diff.transpose() * S.inverse() * z_diff;
  nis_values_laser_file << nis_epsilon << ',' << std::flush;

}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */

  // Make things more readable
  double px, py, psi, v;
  unsigned n_z = 3;
  MatrixXd S = MatrixXd(n_z,n_z);
  //create vector for incoming radar measurement
  VectorXd z = VectorXd(n_z);

  float rho_meas = meas_package.raw_measurements_(0);
  float phi_meas = meas_package.raw_measurements_(1);
  float rhodot_meas = meas_package.raw_measurements_(2);

  z << rho_meas, phi_meas, rhodot_meas;
  //create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_, n_z);

  //create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);

  //transform sigma points into measurement space
  for(int col=0;col<(2*n_aug_+1);col++){
      px = Xsig_pred_(0,col);
      py = Xsig_pred_(1,col);
      v = Xsig_pred_(2,col);
      psi = Xsig_pred_(3,col);

      Zsig(0,col) = sqrt(px*px + py*py);
      Zsig(1,col) = atan2(py, px);
      Zsig(2,col) = (v*(px*cos(psi)+py*sin(psi))) / Zsig(0,col);
  }

  //calculate mean predicted measurement
  z_pred.fill(0.0);
  for (int col=0; col < 2*n_aug_+1; col++) {
      z_pred = z_pred + weights_(col) * Zsig.col(col);
  }

  //calculate innovation covariance matrix S
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }
  //add measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_z,n_z);
  R <<    std_radr_*std_radr_, 0, 0,
          0, std_radphi_*std_radphi_, 0,
          0, 0,std_radrd_*std_radrd_;
  S = S + R;

  //calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  //Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  //residual
  VectorXd z_diff = z - z_pred;

  //angle normalization
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

  //update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();

  // Calculate NIS
  float nis_epsilon = z_diff.transpose() * S.inverse() * z_diff;
  nis_values_radar_file << nis_epsilon << ','<< std::flush;


}

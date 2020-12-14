In order to use this file... we need to make the following changes in 2 files.

**********In Transceiver.h file =>**********

We need to add the below code after enum packetType{.....}:

/********************************************************************************/

typedef enum {
  flag_18DBM = 1,
  flag_12DBM = 2,
  flag_6DBM = 3,
  flag_0DBM = 4
} output_power; 

output_power power_flag;
extern output_power power_flag;     //used to know which output power is being used  

/********************************************************************************/

**********In hal_nrf.c file =>**********

We need to replace a function which is void hal_nrf_set_output_power(hal_nrf_output_power_t power) with the following:

/********************************************************************************/

void hal_nrf_set_output_power(hal_nrf_output_power_t power){

  hal_nrf_write_reg(RF_SETUP, (hal_nrf_read_reg(RF_SETUP) & ~((1<<RF_PWR1)|(1<<RF_PWR0))) | (UINT8(power)<<1));
  if(power == HAL_NRF_0DBM)
  {
                power_flag = flag_0DBM;
  }
  if(power == HAL_NRF_6DBM)
  {
                power_flag = flag_6DBM;
  }
  if(power == HAL_NRF_12DBM)
  {
                power_flag = flag_12DBM;
  }
  if(power == HAL_NRF_18DBM)
  {
                power_flag = flag_18DBM;
  }
}

/* Note: The reason for adding the above statements:
whenever we call this function from anywhere from our project...by using ‘power_flag’ we now can know the power that transmitter is using at any instant.
This power_flag is then used in System_improvement.c file to implement Dynamic output power algorithm to change from one transmitter output power to another transmitter output power.

//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

//Functionality for the MPC106

#ifndef MPC106_H_
#define MPC106_H_

#define mpc106_addres_map_a 1
#define mpc106_addres_map_b 0

extern uint32_t mpc106_address;
extern uint32_t mpc_config_addr;
extern uint32_t mpc_config_dat;

extern uint32_t mpc106_write_word;
extern uint32_t mpc106_read_word;
extern uint16_t mpc106_write_half;
extern uint16_t mpc106_read_half;
extern uint8_t mpc106_write_byte;
extern uint8_t mpc106_read_byte;
extern unsigned char* mpc106_regs;

extern void mpc106_init();
extern void mpc106_read();
extern void mpc106_write(uint32_t write_word);

#endif
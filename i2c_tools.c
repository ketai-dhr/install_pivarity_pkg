#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define log_error(...) fprintf(stderr,__VA_ARGS__)

#define I2C_SLAVE_FORCE 0x0706
typedef signed   char      int8_t;
typedef unsigned char      uint8_t;

typedef signed   short     int16_t;
typedef unsigned short     uint16_t;

// typedef signed   long      int32_t;
typedef unsigned long      uint32_t;

enum I2C_MODE{
	I2C_MODE_8_8,
	I2C_MODE_8_16,
	I2C_MODE_16_8,
	I2C_MODE_16_16,
    I2C_MODE_BLOCK,
};
typedef struct{
    int device_handle;
    enum I2C_MODE i2c_mode;
    uint16_t chip_address;
    uint16_t register_address;
    uint16_t register_value;
    uint16_t block_num;
    uint8_t *buffer;
}I2C_RW_STATE;

void i2c_wr(I2C_RW_STATE *state)
{
	
    if (ioctl(state->device_handle, I2C_SLAVE_FORCE, state->chip_address) < 0)
	{
		log_error("Failed to set I2C address\n");
	}
    unsigned char msg[4];
    int len;

    switch(state->i2c_mode){
    case I2C_MODE_8_8:
        msg[0] = state->register_address;
        msg[1] = state->register_value;
        len = 2;
        break;
    case I2C_MODE_8_16:
        msg[0] = state->register_address;
        msg[1] = state->register_value >> 8;
        msg[2] = state->register_value;
        len = 3;
        break;
    case I2C_MODE_16_8:
        msg[0] = state->register_address >> 8;
        msg[1] = state->register_address;
        msg[2] = state->register_value;
        len = 3;
        break;
    case I2C_MODE_16_16:
        msg[0] = state->register_address >> 8;
        msg[1] = state->register_address;
        msg[2] = state->register_value >> 8;
        msg[3] = state->register_value;
        len = 4;
        break;
    case I2C_MODE_BLOCK:
        if (write(state->device_handle, state->buffer, state->block_num) != state->block_num){
            log_error("Failed to write register \n");
        }
        return;
    default:
        log_error("Invalid parameter I2C_MODE %d\n",state->i2c_mode);
        return;
    }

    if (write(state->device_handle, msg, len) != len){
        log_error("Failed to write register \n");
    }

}

static int i2c_rd(I2C_RW_STATE *state)
{
    int err;
	uint8_t buf[2];
    uint8_t data[2];
    switch(state->i2c_mode){
    case I2C_MODE_8_8:
        buf[0] = state->register_address;
        break;
    case I2C_MODE_8_16:
        buf[0] = state->register_address;
        break;
    case I2C_MODE_16_8:
        buf[0] = state->register_address >> 8;
        buf[1] = state->register_address;
        break;
    case I2C_MODE_16_16:
        buf[0] = state->register_address >> 8;
        buf[1] = state->register_address;
        break;
    default:
        log_error("Invalid parameter I2C_MODE %d\n",state->i2c_mode);
        return -1;
    }

	
	struct i2c_rdwr_ioctl_data msgset;
	struct i2c_msg msgs[2] = {
		{
			 .addr = state->chip_address,
			 .flags = 0,
			 .len = (state->i2c_mode == I2C_MODE_16_8 || state->i2c_mode == I2C_MODE_16_16) ? 2 : 1,
			 .buf = buf,
		},
		{
			.addr = state->chip_address,
			.flags = I2C_M_RD,
			.len = (state->i2c_mode == I2C_MODE_8_16 || state->i2c_mode == I2C_MODE_16_16) ? 2 : 1,
			.buf = data,
		},
	};

	msgset.msgs = msgs;
	msgset.nmsgs = 2;

	err = ioctl(state->device_handle, I2C_RDWR, &msgset);
	// log_error("Read i2c addr %02X, reg %04X (len %d), value %02X, err %d\n", i2c_addr, msgs[0].buf[0], msgs[0].len, values[0], err);
	if (err != (int)msgset.nmsgs)
		return -1;

    if((state->i2c_mode == I2C_MODE_8_16 || state->i2c_mode == I2C_MODE_16_16)){
        state->register_value = 0;
        state->register_value |= data[0];
        state->register_value <<= 8;
        state->register_value |= data[1];
    }else{
        state->register_value = data[0];
    }

	return 0;
}

static void show_helps(char *app_name){
    // char *app_name = (char *)basename(argv[0]);
    log_error("Usage:\n"
                "   %s <i2c_mode(see note.)> <chip_address> <register_addr> [<register_value>]\n"
                "example:\n"
                "   write: ./%s 2-1 0x10 0x0202 0x0F\n"
                "   read: ./%s 2-1 0x10 0x0202\n"
                "I2C_MODE:\n"
                "   I2C_MODE_8_8:   1-1\n"
                "   I2C_MODE_8_16:  1-2\n"
                "   I2C_MODE_16_8:  2-1\n"
                "   I2C_MODE_16_16: 2-2\n",app_name,app_name,app_name);
}

#define READ_FLAG 4
#define WRITE_FLAG 5
#define ARGV_BASE 2
int parse_cmdline(int argc ,char **argv,I2C_RW_STATE *state){
    enum I2C_MODE i2c_mode;
    int chip_address = 0x00;
    int address = -1,value = -1;
    uint8_t *data = NULL;
    uint16_t block_num = -1;
    if(argc < 2){
        return -1;
    }
    // if(isdigit(argv[ARGV_BASE - 1])){
    //     i2c_mode = I2C_MODE_BLOCK;
    //     block_num = strtoul(argv[ARGV_BASE - 1],NULL,10);
    //     data = malloc(block_num);
    //     int i ;
    //     for(i = 0 ; i < block_num ;++i){
    //         data[i] = strtoul(argv[ARGV_BASE + i + 1], NULL, 16);
    //         log_error("0x%02X ",data[i]);
    //     }
    //     log_error("\n");
    // }else 

    if(!strcmp(argv[ARGV_BASE - 1],"1-1")){
        i2c_mode = I2C_MODE_8_8;
    }else if(!strcmp(argv[ARGV_BASE - 1],"1-2")){
        i2c_mode = I2C_MODE_8_16;
    }else if(!strcmp(argv[ARGV_BASE - 1],"2-1")){
        i2c_mode = I2C_MODE_16_8;
    }else if(!strcmp(argv[ARGV_BASE - 1],"2-2")){
        i2c_mode = I2C_MODE_16_16;
    }else{
        int isDigit = 1,i;
        for(i = 0 ; i < sizeof(argv[ARGV_BASE - 1]) ;i++){
            if(isdigit(argv[ARGV_BASE - 1][0])){
                continue;
            }
            isDigit = 0;
            break;
        }
        if(!isDigit){
            return -1;
        }
        block_num = strtoul(argv[ARGV_BASE - 1],NULL,10);
        log_error("block num = %d\n",block_num);

        if(block_num + 3 == argc){
            data = malloc(block_num);
            for(i = 0 ; i < block_num ;++i){
                data[i] = strtoul(argv[ARGV_BASE + i + 1], NULL, 16);
                log_error("0x%02X ",data[i]);
            }
            log_error("\n");
            state->buffer = data;
        }else if(argc == 3){
            
        }else{
            return -1;
        }

        return -1;
    }

    if(argc == READ_FLAG){
        address = strtoul(argv[1 + ARGV_BASE], NULL, 16);
    }else if(argc == WRITE_FLAG){
        address = strtoul(argv[1 + ARGV_BASE], NULL, 16);
        value= strtoul(argv[2 + ARGV_BASE], NULL, 16);
        log_error("input value is %d\n",value);
    }else{
        // show_helps(basename(argv[0]));
        return -1;
    }
    
    chip_address = strtoul(argv[ARGV_BASE], NULL, 16);

    state->chip_address = chip_address;
    state->register_address = address;
    state->register_value = value;
    state->i2c_mode = i2c_mode;
    state->block_num = block_num;
#if 0
    int valid = 1;
    int i;
    for(i = 1 ; i < argc && valid; ++i){
        if (!argv[i])
			continue;

		if (argv[i][0] != '-')
		{
			valid = 0;
			continue;
		}
    }
#endif
    return 0;
}

int main(int argc,char **argv){
    int fd = open("/dev/i2c-10", O_RDWR);
	if (!fd)
	{
		log_error("Couldn't open I2C device\n");
		return -1;
	}
    
    I2C_RW_STATE state = {
        .device_handle = fd,
        .buffer = NULL,
    };
    if(parse_cmdline(argc,argv,&state)){
        show_helps(basename(argv[0]));
        return -1;
    }

    switch(argc){
    case READ_FLAG:
        i2c_rd(&state);
        log_error("read value is: 0x%02X\n",state.register_value); 
        break;
    case WRITE_FLAG:
        i2c_wr(&state);
        break;
    }
   

    return 0;
}

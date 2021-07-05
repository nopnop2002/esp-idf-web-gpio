#define MODE_INPUT   1
#define MODE_OUTPUT  2

#define CMD_SETMODE  100
#define CMD_SETVALUE  200

typedef struct {
	uint16_t command;
	uint16_t pin;
	uint16_t mode;
	uint16_t value;
} GPIO_t;


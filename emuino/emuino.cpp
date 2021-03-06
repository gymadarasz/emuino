#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <stdarg.h>
#include <time.h>

#define __EMU__

// TODO: add it to the wiki
#define SKETCH "sketch/sketch.ino"

// TODO: emulated Arduino device type definition
#define __AVR_ATxmega384D3__ //__DEVICE_TYPE__ - ************** DO NOT REMOVE THIS COMMENT! ITS NEED TO RE-PARSING THIS SOURCECODE !!! *****************
// todo measure the CPU speed or just rewrite the delay.h, interrupt.h etc.. tipical F_CPU values e.g F_CPU=8000000 or F_CPU=1000000UL
#define F_CPU 1000000UL

#include <avr/variants/standard/pins_arduino.h>
#include <avr/cores/arduino/Arduino.h>

// TODO change it if you need, Im not realy sure but I think it's related to Arduino device type
#include <avr/iocanxx.h>

#include <avr/cores/arduino/wiring.c>


class EmuinoFileHandler {
public:
	
	void fappendln(const char fname[], const char msg[]) {
		FILE* f = fopen(fname, "a");
		fprintf(f, "%s\n", msg);
		fclose(f);		
	}
	
	long fsize(const char fname[]) {
		FILE* f = fopen(fname, "r");
		fseek(f, 0L, SEEK_END);
		long size = ftell(f);
		fclose(f);
		return size;
	}
	
	void fclear(const char fname[]) {
		FILE* f = fopen(fname, "w");
		fclose(f);
	}
	
} emuFileHandler;



class EmuinoLogger {
public:
	
	void log(const char msg[]) {
		printf("%s\n", msg);		
		emuFileHandler.fappendln("emuino.log", msg);
	}
	
	void clear() {
		
	}
	
} emuLogger;

class EmuinoClientEventHandler {
public:
	virtual void cliCmd(char* cmdStr);
};


class EmuinoPipe : EmuinoClientEventHandler {
public:
	

	
	void send(char* msg) {
		emuLogger.log("[pipe send]: start:");
		emuLogger.log(msg);
		emuFileHandler.fappendln("../pipe_srv", msg);
		while(emuFileHandler.fsize("../pipe_srv")); // TODO may we need at client sending aldo!!
		emuLogger.log("[pipe send]: finish");
	}
	
	void sendf (const char * format, ... ) {
		char buffer[256];
		va_list args;
		va_start (args, format);
		vsprintf (buffer,format, args);
		send (buffer);
		va_end (args);
	}
		
	void read() {
		//log("[pipe read]: start");	
		FILE* f = fopen("../pipe_cli", "r+");
		bool clearNeeded = false;
		if(f!=NULL) {
			char line[255];
			while (fgets(line, sizeof(line), f)) {
				cliCmd(line);
				clearNeeded = true;
			}
			fclose(f);		
			if(clearNeeded) {
				clear();
			}
			//log("[pipe read]: finish");
		}
	}
	
	void clear() {
		fclose(fopen("../pipe_cli", "w"));
	}
	
	void cliCmd(char* cmdStr) {
		emuLogger.log("[pipe cli cmd]: run:");
		emuLogger.log(cmdStr);
		// todo: run client command..
	}
};




class Sketch {
public:
#include SKETCH
} sketch;





class Emuino : public EmuinoPipe {
private:

	int id;
	
	int pinModes[NUM_DIGITAL_PINS+NUM_ANALOG_INPUTS];
	int pinValues[NUM_DIGITAL_PINS+NUM_ANALOG_INPUTS];
	
	
	void reset() {
		emuLogger.log("[arduino]: reset..");
		// todo: reset pins and all of emulated arduino
		for(int i=0; i<NUM_DIGITAL_PINS+NUM_ANALOG_INPUTS; i++) {
			setPinMode(i, 0);
			setPinValue(i, 0);
		}
	}
	

public:
	
	Emuino() {
		srand(time(NULL));
		id = rand();
		fclose(fopen("../pipe_cli", "w"));
		emuLogger.clear();
		emuLogger.log("[EMUINO] start");
		sendf("emuino.make('Arduino', '%d', {});", id);
		reset();
		emuLogger.log("[emuino sketch]: setup..");
		sketch.setup();
		emuLogger.log("[emuino sketch]: loop start.. (press a key to stop)");
		while(!kbhit()) {
			read();
			sketch.loop();
		}
		emuLogger.log("[EMUINO] halt");
		getch();
		getch();
	}
	
	void setPinMode(int pin, int mode) {
		pinModes[pin] = mode;
		sendf("devices.Arduino[%d].setPinMode(%d, %d);", id, pin, mode);
	}
	
	void setPinValue(int pin, int value) {
		pinValues[pin] = value;
		sendf("devices.Arduino[%d].setPinValue(%d, %d);", id, pin, value);
	}
		
	int getStrArrVal(int idx, char str[]) {
		int i=0, val;
		char sepa = ',';
		if(idx<0) throw "negative array index?! hmm...";
		while(!(str[i]==0||str[i]=='\n') && i<255){
			if(idx==0) {
				val = 0;
				while(!(str[i]==0||str[i]=='\n'||str[i]==sepa) && i<255) {
					val = val*10 + (str[i]-'0');
					i++;
				}
				// todo check that its numeric
				return val;
			}
			if(str[i]==sepa) {
				idx--;
			}
			i++;
		}
		// todo ?? no value..
	}
	
	void cliCmd(char* cmdStr) {
		EmuinoPipe::cliCmd(cmdStr);
		printf("dx1(%s)",cmdStr);
		
		// TODO parse cmd array here!! format: "cmd,id,arg0,arg1,arg..." todo check wssd project: do we need to take a \n to end of lines??
		int id = getStrArrVal(1, cmdStr);
		printf("dx2(%s)",cmdStr);
		if(id != this->id) {
			emuLogger.log("invalid client id (hmm.. may a client doesn't closed correctly?!')");
		}
		else {
			printf("dx3(%s)",cmdStr);
			int cmd = getStrArrVal(0, cmdStr);
			printf("dx4(%s)",cmdStr);
			switch(cmd) {
				case 0: // setPinValue
				{
					printf("dx5(%s)",cmdStr);
					int pin = getStrArrVal(2, cmdStr);
					printf("dx6(%s)",cmdStr);
					int value = getStrArrVal(3, cmdStr);
					setPinValue(pin, value);
					break;
				}
					
				default:
					emuLogger.log("invalid command");
			}
		}		
	}
	
	~Emuino() {		
		sendf("emuino.remove('Arduino', '%d', {});", id);
		emuLogger.log("[EMUINO] end");
	}
} emu;


#include <avr/cores/arduino/wiring_digital.c>


/* run this program using the console pauser or add your own getch, system("pause") or input loop */

int main(int argc, char** argv) {
	
	return 0;
}



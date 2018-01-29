#include "Time.h"

int Time::Timeout_end(int number){
	if(timeout_start[number] == true ){
		unsigned long current_time = millis();
		unsigned long time_difference =  current_time -timeout_start_time[number];
		if(time_difference >= timeout_holder[number]){
			*timeout_function[number] = false;
			timeout_start[number] = false;
		}
		return time_difference;
	}
}

int Time::Timeout_start(bool &var_to_false, int number, int timeout){
	if(timeout_start[number] == false){
		timeout_function[number] = &var_to_false;
		timeout_holder[number] = timeout;
		timeout_start[number] = true;
		timeout_start_time[number] = millis();
		return -1;
	}
	Timeout_end(number);
}

/* code example {start}:
	void loop(){ // or main()
		Timeout_manager();
		...
		Random_function();
		...
	}
code example {start}. */

void Time::Timeout(bool &var_to_false, void (Function_to_do_once()), int number, int timeout){
	if (var_to_false == false){
		var_to_false = true;
		Timeout_start(var_to_false, number, timeout);
		/* code that you timeout {start}*/
			Function_to_do_once();
		/* code that you timeout {end}*/
		return;
	}
}


void Time::Timeout_manager(){
	for(int i = 0; i < max_timeout_number; i++){
		Timeout_end(i);
	}
	return;
}


/*
Function to call when interval time has past (in milliseconds))
*/
int Time::Timer(void Function_to_control(), int function_number, long update_interval = 0, int multiplyer = 1000){
    // array size defines how many different Timer functions can be called
    static unsigned long previous_micros_array[max_timer_number];
    update_interval = update_interval * multiplyer;
    unsigned long current_micros = micros();
    unsigned long time_difference = current_micros - previous_micros_array[function_number];
    if(time_difference >= update_interval){
        Function_to_control();
        previous_micros_array[function_number] = current_micros;
    }
    return time_difference;
}
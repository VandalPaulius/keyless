#ifndef TIME_H
#define TIME_H

	#include "Arduino.h"
	// Time class global variables
	#define max_timeout_number 10
	#define max_timer_number 10

	class Time{
		private:
			// variables
			//#define max_timeout_number 10
			bool timeout_start[max_timeout_number];
			unsigned long timeout_start_time[max_timeout_number];
			int timeout_holder[max_timeout_number];
			bool *timeout_function[max_timeout_number];
			
			// functions
			int Timeout_end(int);
			int Timeout_start(bool&, int, int);

		public:
			// functions
			void Timeout(bool&, void (Function_to_do_once()), int, int);
			void Timeout_manager();
			int Timer(void Function_to_control(), int, long, int);
	};

#endif
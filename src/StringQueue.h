/*	Copyright 2016 DeviceDrive AS
*
*	Licensed under the Apache License, Version 2.0 (the "License");
*	you may not use this file except in compliance with the License.
*	You may obtain a copy of the License at
*
*	http ://www.apache.org/licenses/LICENSE-2.0
*
*	Unless required by applicable law or agreed to in writing, software
*	distributed under the License is distributed on an "AS IS" BASIS,
*	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*	See the License for the specific language governing permissions and
*	limitations under the License.
*
*/

#pragma once
#include <Arduino.h>

#define MAX_SIZE 10

class StringQueue
{
	public:
		StringQueue();
		bool empty();
		int size();
		int count();
		void clear();

		bool push_back(String item);
		bool push_front(String item);

		String front();
		String back();
		String pop_front();


	private:
		int item_count;
		String list[MAX_SIZE];
		int first_pos;
		int last_pos;
		int add_pos;

		void increment(int &value);
		void decrement(int &value);
};
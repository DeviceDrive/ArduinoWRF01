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

#include "StringQueue.h"

StringQueue::StringQueue()
{
	first_pos = last_pos = 0;
	item_count = 0;
	add_pos = 0;
}

bool StringQueue::empty()
{
	return item_count == 0;
}

int StringQueue::size()
{
	return MAX_SIZE;
}

int StringQueue::count()
{
	return item_count;
}

void StringQueue::clear()
{
	first_pos = last_pos = 0;
	item_count = 0;
	add_pos = 0;
	for (int i = 0; i < MAX_SIZE; i++) {
		list[i] = "";
	}
}

bool StringQueue::push_back(String item)
{
	if (item_count < MAX_SIZE) {
		last_pos = add_pos;
		list[add_pos] = item;
		increment(add_pos);
		item_count++;
		return true;
	}
	return false;

}

bool StringQueue::push_front(String item)
{
	if (item_count < MAX_SIZE) {
		decrement(first_pos);
		list[first_pos] = item;
		item_count++;
		return true;
	}
	return false;
}

String StringQueue::front()
{
	if (empty()) return "";
	else return list[first_pos];
}

String StringQueue::back()
{
	if (empty()) return "";
	else return list[last_pos];
}

String StringQueue::pop_front()
{
	if (!empty()) {
		String item = list[first_pos];
		increment(first_pos);
		item_count--;
		return item;
	}
	else
		return "";
}

void StringQueue::increment(int & value)
{
	value++;
	if (value >= MAX_SIZE) value = 0;

}

void StringQueue::decrement(int & value)
{
	value--;
	if (value < 0) value = MAX_SIZE - 1;
}


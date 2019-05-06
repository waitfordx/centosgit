/*==============================================
* FileName : timer.cpp
* Author   : liming
* Create Time :2019-05-06
* describe : 
==============================================*/
#include <stdio.h>
#include <iostream>
#include <event.h>

using namespace std;

void OnTime(int sock, short event, void *arg)
{
	cout<<"time is up!"<<endl;

	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	event_add((struct event*)arg, &tv);
}

int main ()
{
	event_init();

	struct event evTimer;
	evtimer_set(&evTimer, OnTime, &evTimer);

	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	event_add(&evTimer, &tv);

	event_dispatch();
	return 0;




}

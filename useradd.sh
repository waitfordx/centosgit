#!/bin/bash

#==================================================
#File Name   : useradd.sh
#Author      :  liming 
#Creat Time  : 2019-05-06
#Discription : 
#
#==================================================

groupadd lm_group
for username in lm_user1 lm_user2 lm_user2 lm_user3 lm_user4 lm_user5 
	do 
		useradd -G lm_group $username
		echo  "password " | passwd --stdin $username
	done

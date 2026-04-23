#!/bin/bash

for entry in *
do
	if [ -d $entry ]
	then
		echo
		echo "${entry}:"

		cd $entry
		make
		cd ..
	fi
done

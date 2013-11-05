#!/bin/bash

#echo "Enter the absolute directory path of the xmlrpc_hello_server.o"
#read path
path="/home/jay/Desktop/xml_rpc/backup/xmlrpc-c/examples"
program="xmlrpc_sample_add_server"

function quit
{
	echo "Closing all servers and performing cleanup before the program terminate..."
	pid_array=($(pidof $program))
	#echo "length: "${#pid_array[@]}
	let length=${#pid_array[@]}-1 
	#echo $length
	for i in $(seq 0 $length)
	do
		echo ${pid_array[$i]}
		kill -SIGKILL ${pid_array[$i]}
	done
	exit
}

echo "What terminal command does your machine support?"
select choice in "XTERM" "GNOME" "KONSOLE";
do
	case $choice in 
		XTERM ) echo "Will be using XTERM to create multiple servers"
			cmd_head="xterm -e";
			cmd_tail="&"
			break
			;;
		GNOME ) echo "Will be using GNOME to create multiple servers"
			cmd_head="gnome-terminal -x";
			cmd_tail=""
			break
			;;
		KONSOLE ) echo "Will be using KONSOLE to create multiple servers"
			cmd_head="konsole --new-tab --hold -e";
			cmd_tail=""
			break
			;;
	esac
done

echo "Enter the number of servers to run:"
read num_server
echo "Running $num_server servers..."

port_num=7070
for i in $(seq 1 $num_server)
do
	echo "Running server:#$i on port:$port_num"
	#echo "Enter port number for server#$i"
	#read port_num
	$cmd_head $path/$program $port_num >/dev/null 2>&1 $cmd_tail 
	let port_num=port_num+1
	#port_num=$(($port_num+1))
	#let "port_num += 1"
done

echo "Done creating all servers. Press q to quit."
while true;
do
	read -s -n 1 quit_key
	#echo "reading $quit_key"
	if [[ $quit_key = "q" ]]; then
		quit
	else 
		echo "Press q to quit."
	fi
done



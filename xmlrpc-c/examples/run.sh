#!/bin/bash

echo "Enter the absolute directory path of the xmlrpc_hello_server.o"
read path

echo "What terminal command does your machine support?"
select choice in "XTERM" "GNOME" "KONSOLE";
do
	case $choice in 
		XTERM ) echo "Will be using XTERM to create multiple servers"
			term="xterm -hold -e"
			break
			;;
		GNOME ) echo "Will be using GNOME to create multiple servers"
			term="gnome-terminal -x"
			break
			;;
		KONSOLE ) echo "Will be using KONSOLE to create multiple servers"
			term="konsole --hold -e"
			break
			;;
	esac
done

echo "Enter the number of servers to run"
read num_server
echo "Running $num_server servers..."

port_num=7070
for i in $(seq 1 $num_server)
do
	echo "Running server:#$i on port:$port_num"
	#echo "Enter port number for server#$i"
	#read port_num
	$term $path/xmlrpc_hello_server $port_num >/dev/null 2>&1
	#konsole --hold -e $path/xmlrpc_sample_add_server $port_num >/dev/null 2>&1
	let port_num=port_num+1
	#port_num=$(($port_num+1))
	#let "port_num += 1"
done

#while:
#do
#	read -t 1 -n 1 key
#	if([[$key = q]]) then
#		quit
#	fi
#done

function quit
{
	echo "Closing all servers and performing cleanup before the program terminate..."
	exit
}

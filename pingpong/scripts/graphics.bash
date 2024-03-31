#!/bin/bash

#-e:  Exit immediately if a command exits with a non-zero status.
set -e

readonly DataDir='../data' 
readonly TcpThroughput="/tcp_throughput.dat"
readonly UdpThroughput="/udp_throughput.dat"

if [[ ! -r ${DataDir}${TcpThroughput} || ! -r ${DataDir}${UdpThroughput} ]]; then
	echo "Cannot read input files"
	exit
fi

#prendo i valori da tcp_throughput.dat
N1=$(head -n 1 ${DataDir}${TcpThroughput} | cut -d ' ' -f 1)
T1=$(head -n 1 ${DataDir}${TcpThroughput} | cut -d ' ' -f 2)
N2=$(tail -n 1 ${DataDir}${TcpThroughput} | cut -d ' ' -f 1) 
T2=$(tail -n 1 ${DataDir}${TcpThroughput} | cut -d ' ' -f 2)

#calcolo i delay
D_N1=$(echo "scale=9; $N1/$T1" | bc -l)
D_N2=$(echo "scale=9; $N2/$T2" | bc -l)

#calcolo banda e latenza
myB=$(echo "scale=9; ($N2-$N1)/($D_N2-$D_N1)" | bc -l)
myL=$(echo "scale=9; ($D_N1*$N2 - $D_N2*$N1)/($N2-$N1)" | bc -l)

#chiamo gnuplot per il grafico TCP
gnuplot <<-eNDgNUPLOTcOMMAND
	set term png size 900, 700
	set output "../data/graphic_tcp.png"
	set logscale x 2
	set logscale y 10
	set xlabel "msg size (B)"
	set ylabel "throughput (KB/s)"
	lbf(x) = x / ( $myL + x / $myB )
	plot lbf(x) title "TCP Latency Bandwidth model with L=$myL and B=$myB"	\
		with linespoints, \
	 "${DataDir}${TcpThroughput}" using 1:2 title "TCP median Throughput" \
		  with linespoints	
	clear
eNDgNUPLOTcOMMAND

#La ripetizione di comandi uguali non va bene; ci vuole un controllo su quale dei due file stiamo usando e poi
#usare i comandi una sola volta

#riprendo i valori, ma questa volta da udp
N1=$(head -n 1 ${DataDir}${UdpThroughput} | cut -d ' ' -f 1)
T1=$(head -n 1 ${DataDir}${UdpThroughput} | cut -d ' ' -f 2)
N2=$(tail -n 1 ${DataDir}${UdpThroughput} | cut -d ' ' -f 1)
T2=$(tail -n 1 ${DataDir}${UdpThroughput} | cut -d ' ' -f 2)
#Bisogna controllare il caso in cui il valore del Throughput sia scritto in notazione esponenziale
SUB='e'
#Se T2 contiene la lettera 'e'
if [[ "$T2" == *"$SUB"* ]]; then
	#Separa base ed esponente e calcola il valore finale
  T2base=$(tail -n 1 ${DataDir}${UdpThroughput} | cut -d ' ' -f 2 | cut -d 'e' -f 1)
  T2exp=$(tail -n 1 ${DataDir}${UdpThroughput} | cut -d ' ' -f 2 | cut -d '+' -f 2)
  T2=$(echo "$T2base*(10^$T2exp)" | bc -l )
  #Salva T2 come intero senza virgola
  T2=$(echo "$T2" | awk '{printf( "%.f", $1)}')
fi

#echo "$T2base, $T2exp" 
#echo "$N1 , $T1 , $N2 , $T2"

#calcolo i delay
D_N1=$(echo "scale=9; $N1/$T1" | bc -l)
D_N2=$(echo "scale=9; $N2/$T2" | bc -l) 

#calcolo banda e latenza
myB=$(echo "scale=9; ($N2-$N1)/($D_N2-$D_N1)" | bc)
myL=$(echo "scale=9; ($D_N1*$N2 - $D_N2*$N1)/($N2-$N1)" | bc)

#echo "B=$myB, L=$myL"

#chiamo gnuplot per il grafico UDP
gnuplot <<-eNDgNUPLOTcOMMAND
	set term png size 900, 700
	set output "../data/graphic_udp.png"
	set logscale x 2
	set logscale y 10
	set xlabel "msg size (B)"
	set ylabel "throughput (KB/s)"
	lbf(x) = x / ( $myL + x / $myB )
	plot lbf(x) title "UDP Latency Bandwidth model with L=$myL and B=$myB" \
		with linespoints, \
	 "${DataDir}${UdpThroughput}" using 1:2 title "UDP median Throughput" \
		 with linespoints
	clear
eNDgNUPLOTcOMMAND


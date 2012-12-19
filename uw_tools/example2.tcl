#create a simulator object
set ns [new Simulator]

#open the nam trace file
set nf [open data/example2.out.nam w]
$ns namtrace-all $nf

#define color
$ns color 1 Blue
$ns color 2 Red

#define a 'finish' procedure
proc finish {} {
    global ns nf
    $ns flush-trace
    #close trace file
    close $nf
    #exec nam on the trace file
    exec nam out.nam &
    exit 0
}
#create two nodes
set n0 [$ns node]
set n1 [$ns node]
set n3 [$ns node]
set n2 [$ns node]

#create a duplex link between the nodes
$ns duplex-link $n0 $n2 1Mb 10ms DropTail
$ns duplex-link $n1 $n2 1Mb 10ms DropTail
$ns duplex-link $n3 $n2 1Mb 10ms DropTail

#
$ns duplex-link-op $n0 $n2 orient right-down
$ns duplex-link-op $n1 $n2 orient right-up
$ns duplex-link-op $n2 $n3 orient right

#monitor the queue
$ns duplex-link-op $n3 $n2 queuePos 0.5
$ns duplex-link $n2 $n3 1Mb 10ms SFQ

#create a UDP agent and attach it to node n0
set udp0 [new Agent/UDP]
$ns attach-agent $n0 $udp0

#create a CBR traffic source and attach it to udp0
set cbr0 [new Application/Traffic/CBR]
$cbr0 set packetSize_ 500
$cbr0 set interval_ 0.005
$cbr0 attach-agent $udp0
$udp0 set class_ 1

#create a UDP agent and attach it to node n1
set udp1 [new Agent/UDP]
$ns attach-agent $n1 $udp1

#create a CBR traffic source and attach it to upd1
set cbr1 [new Application/Traffic/CBR]
$cbr1 set packetSize_ 500
$cbr1 set interval_ 0.005
$cbr1 attach-agent $udp1
$udp1 set class_ 2
#create a Null agent which acts as traffic sink and attach it to node n1
set null0 [new Agent/Null]
$ns attach-agent $n3 $null0

#connect traffic source of n0 and n3 
$ns connect $udp0 $null0
#connect traffic source of n1 and n3
$ns connect $udp1 $null0

#call the finish procedure after 5 seconds of simulation time
$ns at 5.0 "finish"

#when to send data and when to stop
$ns at 0.5 "$cbr0 start"
$ns at 1.0 "$cbr1 start"
$ns at 4.0 "$cbr1 stop"
$ns at 4.5 "$cbr0 stop"

#run the simulation
$ns run

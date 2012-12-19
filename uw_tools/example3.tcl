#create a simulator object
set ns [new Simulator]
$ns rtproto DV
#open the nam trace file
set nf [open data/example3.out.nam w]
$ns namtrace-all $nf

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
#create 7 nodes
for {set i 0} {$i < 7} {incr i} {
    set n($i) [$ns node]
}

#connect the nodes to create a circular topology
for {set i 0} {$i < 7 } {incr i} {
    $ns duplex-link $n($i) $n([expr ($i+1)%7]) 1Mb 10ms DropTail
}

#create a UDP agent and attach it to node no
set udp0 [new Agent/UDP]
$ns attach-agent $n(0) $udp0

#create a CBR traffic source and attach it to udp0
set cbr0 [new Application/Traffic/CBR]
$cbr0 set packageSize_ 500
$cbr0 set interval_ 0.005
$cbr0 attach-agent $udp0

#create a Null agent which acts as traffic sink and attach it to node n3
set null0 [new Agent/Null]
$ns attach-agent $n(3) $null0

#connect the two agent
$ns connect $udp0 $null0

#let the link between node1 and node2 go down
$ns rtmodel-at 1.0 down $n(1) $n(2)
$ns rtmodel-at 2.0 up   $n(1) $n(2)

#call the finish procedure after 5 seconds of simulation time
$ns at 5.0 "finish"

#when to send data and when to stop
$ns at 0.5 "$cbr0 start"
$ns at 4.5 "$cbr0 stop"

#run the simulation
$ns run

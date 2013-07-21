set ns [new Simulator]

#recording data in output files
set f0 [open data/test_xgraph0.tr w]
set f1 [open data/test_xgraph1.tr w]
set f2 [open data/test_xgraph2.tr w]

proc finish {} {
    global f0 f1 f2
	set ns [Simulator instance]
    $ns flush-trace
    close $f0
    close $f1
    close $f2
    exec xgraph data/test_xgraph0.tr data/test_xgraph1.tr data/test_xgraph2.tr -geometry 100*100 &
    exit 0
}

set n0 [$ns node]
set n1 [$ns node]
set n2 [$ns node]
set n3 [$ns node]
set n4 [$ns node]

$ns duplex-link $n0 $n3 1Mb 100ms DropTail
$ns duplex-link $n1 $n3 1Mb 100ms DropTail
$ns duplex-link $n2 $n3 1Mb 100ms DropTail
$ns duplex-link $n3 $n4 1Mb 100ms DropTail

#proc to add traffic sources and generators to the nodes
proc attach-expoo-traffic { node sink size burst idle rate } {
    #get an instance of the simulator
    set ns [Simulator instance]
    #create a UDP agent and attach it to the node
	#用udp来产生数据，就像uwsink一样
    set source [new Agent/UDP]
    $ns attach-agent $node $source

    #create an expoo traffic agent and set its config parameters
    set traffic [new Application/Traffic/Exponential]
    $traffic set packetSize_ $size
    $traffic set burst_time_ $burst
    $traffic set idle_time_ $idle
    $traffic set rate_  $rate

    #attach traffic source to the traffic generator
    $traffic attach-agent $source
    #connect the source and the sink
    $ns connect $source $sink
    return $traffic
}

set sink0 [new Agent/LossMonitor]
set sink1 [new Agent/LossMonitor]
set sink2 [new Agent/LossMonitor]

$ns attach-agent $n4 $sink0
$ns attach-agent $n4 $sink1
$ns attach-agent $n4 $sink2

set source0 [attach-expoo-traffic $n0 $sink0 200 2s 1s 100k]
set source1 [attach-expoo-traffic $n1 $sink1 200 2s 1s 200k]
set source2 [attach-expoo-traffic $n2 $sink2 200 2s 1s 300k]


#actually writes the data to the output files
proc record {} {
    global sink0 sink1 sink2 f0 f1 f2 f3
    set ns [ Simulator instance]
    set time 0.5
    #how many bytes have been received by the traffic sinks
    set bw0 [$sink0 set bytes_]
    set bw1 [$sink1 set bytes_]
    set bw2 [$sink2 set bytes_]
    set now [$ns now]
    #caculate the bindwidth (in MBit/s) and write it to the files
    puts $f0 "$now [expr $bw0/$time*8/1000000]"
    puts $f1 "$now [expr $bw1/$time*8/1000000]"
    puts $f2 "$now [expr $bw2/$time*8/1000000]"
    #reset the bytes_ values on the traffic sinks
    $sink0 set bytes_ 0
    $sink1 set bytes_ 0
    $sink2 set bytes_ 0
    #re-schedule the procedure，循环调用该过程
    $ns at [expr $now + $time] "record"
}

$ns at 0.0 "record"
$ns at 10.0 "$source0 start"
$ns at 10.0 "$source1 start"
$ns at 10.0 "$source2 start"
$ns at 50.0 "$source0 stop"
$ns at 50.0 "$source1 stop"
$ns at 50.0 "$source2 stop"
$ns at 60.0 "finish"


$ns run

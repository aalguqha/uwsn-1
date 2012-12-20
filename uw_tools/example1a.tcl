set ns [new Simulator]
set nf [open data/example1a.out.nam w]
set xf [open data/example1a.tr w]
$ns namtrace-all $nf


proc record {} {
    global xf sink0
    set ns [Simulator instance]
    set time 0.5
    set bw0 [$sink0 set bytes_]
    set now [$ns now]
    puts $xf "$now [expr $bw0/$time*8/1000000]"
    #$sink0 set bytes_ 0
    $ns at [ expr $now+$time ] "record"
}

proc finish {} {
    global ns nf
    $ns flush-trace
    #Close the trace file
    close $nf
    #Execute nam on the trace file
    #exec nam data/example1a.out.nam &
    exec xgraph data/example1a.tr -geometry 400*300 &
    exit 0
}

set n0 [$ns node]
set n1 [$ns node]

$ns duplex-link $n0 $n1 0.5Mb 10ms DropTail

set udp0 [new Agent/UDP]
$ns attach-agent $n0 $udp0
set cbr0 [new Application/Traffic/CBR]
$cbr0 set packetSize_ 500
$cbr0 set interval_ 0.05
$cbr0 attach-agent $udp0

set sink0 [new Agent/LossMonitor]
$ns attach-agent $n1 $sink0
$ns connect $udp0 $sink0

$ns at 10.0 "finish"
$ns at 0.0 "record"
$ns at 0.5 "$cbr0 start"
$ns at 9.5 "$cbr0 stop"
$ns run



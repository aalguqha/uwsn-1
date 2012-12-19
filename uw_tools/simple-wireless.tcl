#==================
#define options
#===================

#set val(chan)   Channel/UnderwaterChannel   ;#channel type
#set val(netif)  Phy/UnderwaterPhy           ;#network interface type
#set val(prop)   Propagation/Underwater      ;#radio-propagation model


set val(chan)   Channel/WirelessChannel     ;#channel type
set val(netif)  Phy/WirelessPhy             ;#network interface type
set val(prop)   Propagation/TwoRayGround    ;#radio-propagation model
set val(ant)    Antenna/OmniAntenna         ;#antenna type
set val(ll)     LL                          ;#link layer type
set val(ifq)    Queue/DropTail/PriQueue     ;#interface queue type
set val(ifqlen) 50                          ;#max packet in ifq
set val(mac)    Mac/802_11                  ;#MAC type
set val(rp)     DSDV                        ;#ad-hoc routing protocol
set val(nn)     2                           ;#number of mobilenodes

set ns_     [new Simulator]
set tracefd [open data/simple-wireless.tr w]
$ns_ trace-all $tracefd
set topo    [new Topography]

#the mobilenodes move within a topology of 500*500 m
$topo load_flatgrid 500 500
#create object God
create-god $val(nn)
#configure nodes
$ns_ node-config -adhocRouting $val(rp) \
                 -llType $val(ll) \
                 -macType $val(mac) \
                 -ifqType $val(ifq) \
                 -ifqLen  $val(ifqlen) \
                 -antType $val(ant) \
                 -propType $val(prop) \
                 -phyType $val(netif) \
                 -topoInstance $topo \
                 -channelType $val(chan) \
                 -agentTrace ON \
                 -routerTrace ON \
                 -macTrace OFF \
                 -movementTrace OFF

for {set i 0} {$i < $val(nn)} {incr i} {
    set node_($i) [ $ns_ node ]
    $node_($i) random-motion 0      ;#disable random motion
    
}

#provid initial (X,Y, Z=0) co-ordinates for node_(0) and node_(1)
$node_(0) set X_ 5.0
$node_(0) set Y_ 2.0
$node_(0) set Z_ 0.0

$node_(1) set X_ 390.0
$node_(1) set Y_ 385.0
$node_(1) set Z_ 0.0

#produce some node movements
#node_(1) starts to move towards node_(0)
$ns_ at 50.0 "$node_(1) setdest 25.0 20.0 15.0"
$ns_ at 10.0 "$node_(0) setdest 20.0 18.0 1.0"

#node_(0) then starts to move away from node_(0)
$ns_ at 100.0 "$node_(1) setdest 490.0 480.0 15.0"

#TCP connections between node_(0) and node_(1)
set tcp [new Agent/TCP]
$tcp set class_ 2
set sink [new Agent/TCPSink]
$ns_ attach-agent $node_(0) $tcp
$ns_ attach-agent $node_(1) $sink
$ns_ connect $tcp $sink
set ftp [new Application/FTP]
$ftp attach-agent $tcp
$ns_ at 10.0 "$ftp start"

#tell nodes when the simulations ends
for { set i 0} { $i < $val(nn) } {incr i} {
    $ns_ at 150.0 "$node_($i) reset";
}
$ns_ at 150.0001 "stop"
$ns_ at 150.0002 "puts \"NS Exiting...\"; $ns_ halt"
proc stop {} {
    global ns_ tracefd
    close $tracefd
}

#start
puts "Starting simulation"
$ns_ run




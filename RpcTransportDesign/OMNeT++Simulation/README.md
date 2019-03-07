# Homa: A Low Latency, Receiver Driven, Transport Mechanism For Datacenter Networks

## What is Homa?
Homa is a new datacenter transport protocol that provides low latency for small
messages while also supporting large messages and high network utilization. In
Homa, incoming packets are scheduled by receivers in order to prioritize shorter
messages and manage congestion; senders transmit small amounts of unscheduled
data to avoid an extra round-trip of latency for scheduling. Homa uses
in-network priority queues for efficient preemption and, more importantly, to
implement a form of bipartite matching between senders and receivers, which
ensures high network utilization. 

Homa provides low latency for small messages, even in the presence of high
network loads and competing large messages. Across a wide range of workloads,
and even at 80% network load, Homa provides 99th-percentile tail latencies for
short messages within a factor of 2.2x of an unloaded network. Furthermore, Homa
can push the network bandwidth utilization to 90% for different workloads.


## What does this repository provide?
This repository provides the simulation code for Homa. Homa simulation is built
on top of OMNeT++ event simulator. In order to use Homa simulator, first you
need to familiarize yourself with OMNeT++ simulator. We suggest that you refer
to [OMNET++ Documentation](https://www.omnetpp.org/documentation) and [OMNeT++
TicToc tutorial](https://docs.omnetpp.org/) to learn about this simulator.

Once learned how to build and run simulation programs with OMNeT++, clone this
repository and follow the steps below to build the Homa simulation software: 

1. Install omnet++ version 4.6 under the directory ./omnetpp-4.6/

2. Copy the diff patch below into a file (eg. patch.diff) and apply it to OMNeT++ directory and
rebuild OMNeT++ from the directory (.../omnetpp-4.6$ patch -p1 < patch.diff)

3. Enter inet/ directory and build inet framework.

4. Enter homatransport directory and build homatransport executable file

## The patch for OMNeT++ 

        diff --git a/include/cdensityestbase.h b/include/cdensityestbase.h
        index 49b0c01..4b8ea8a 100644
        --- a/include/cdensityestbase.h
        +++ b/include/cdensityestbase.h
        @@ -232,7 +232,7 @@ class SIM_API cDensityEstBase : public cStdDev
              * being the range of the 100 pre-collected values extended 1.3 times
              * symmetrically.
              */
        -    virtual void setRangeAuto(int num_firstvals=100, double range_ext_fact=2.0);
        +    virtual void setRangeAuto(int num_firstvals=100, double range_ext_fact=1.0);

             /**
              * Selects a histogram range setup method where the upper bound of the range
        @@ -241,7 +241,7 @@ class SIM_API cDensityEstBase : public cStdDev
              * The lower bound is calculated by extending the range (minimum of observations, upper)
              * range_ext_fact times.
              */
        -    virtual void setRangeAutoLower(double upper, int num_firstvals=100, double range_ext_fact=2.0);
        +    virtual void setRangeAutoLower(double upper, int num_firstvals=100, double range_ext_fact=1.0);

             /**
              * Selects a histogram range setup method where the lower bound of the range
        @@ -250,7 +250,7 @@ class SIM_API cDensityEstBase : public cStdDev
              * The upper bound is calculated by extending the range (lower, maximum of observations)
              * range_ext_fact times.
              */
        -    virtual void setRangeAutoUpper(double lower, int num_firstvals=100, double range_ext_fact=2.0);
        +    virtual void setRangeAutoUpper(double lower, int num_firstvals=100, double range_ext_fact=1.0);

             /**
              * Sets the number of values to be pre-collected before transformation takes
        diff --git a/src/sim/cdensityestbase.cc b/src/sim/cdensityestbase.cc
        index 25261e9..27e4863 100644
        --- a/src/sim/cdensityestbase.cc
        +++ b/src/sim/cdensityestbase.cc
        @@ -38,9 +38,9 @@ using std::ostream;

         cDensityEstBase::cDensityEstBase(const char *name) : cStdDev(name)
         {
        -    range_mode = RANGE_AUTO;
        -    num_firstvals = 100;
        -    range_ext_factor = 2.0;
        +    range_mode = RANGE_AUTOUPPER;
        +    num_firstvals = 10000;
        +    range_ext_factor = 1.0;
             rangemin = rangemax = 0;
             cell_under = cell_over = 0;
             transfd = false;
        @@ -179,8 +179,8 @@ void cDensityEstBase::clearResult()

             transfd = false;
             range_mode = RANGE_AUTO;
        -    num_firstvals = 100;
        -    range_ext_factor = 2.0;
        +    num_firstvals = 10000;
        +    range_ext_factor = 1.0;
             rangemin = rangemax = 0;
             cell_under = cell_over = 0;

        diff --git a/src/sim/chistogram.cc b/src/sim/chistogram.cc
        index 220c3d9..24f6f51 100644
        --- a/src/sim/chistogram.cc
        +++ b/src/sim/chistogram.cc
        @@ -365,7 +365,7 @@ void cHistogram::setupRangeInteger()
         void cHistogram::setupRangeDouble()
         {
             if (num_cells == -1)
        -        num_cells = 30; // to allow merging every 2, 3, 5, 6 adjacent cells in post-processing
        +        num_cells = 100; // to allow merging every 2, 3, 5, 6 adjacent cells in post-processing
             cellsize = (rangemax - rangemin) / num_cells;
         }

## Running Experiments
After you build the simulation package, you need to go to RpcTransportDesign/OMNeT++Simulation/homatransport/src/dcntopo folder and run your simulation scenario from there. 'dcntopo' folder has most of configurations files we've used for simulations. Here is an example on how to run an experiment configurations:

../homatransport -u Cmdenv -c WorkloadHadoop -r 6 -n ..:../../simulations:../../../inet/examples:../../../inet/src -l ../../../inet/src/INET homaTransportConfig.ini

"-u Cmdenv" instructs OMNeT++ to run in cli mode (as opposed to gui mode). homaTransportConfig.ini at the end of the command is the configuration file we use and "-c WorkoaldHadoop" asks omnet to use parameters specified in WorkloadHadoop section of the config file. -r 6 specifies run number 6 within that section to be simulated.

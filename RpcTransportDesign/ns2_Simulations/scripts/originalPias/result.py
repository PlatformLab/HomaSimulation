import os
import sys
import string
import argparse

#Get average FCT
def avg(flows):
	sum=0.0
	for f in flows:
		sum=sum+f
	if len(flows)>0:
		return sum/len(flows)
	else:
		return 0

#GET mean FCT
def mean(flows):
	flows.sort()
	if len(flows)>0:
		return flows[50*len(flows)/100]
	else:
		return 0

#GET 99% FCT
def max(flows):
	flows.sort()
	if len(flows)>0:
		return flows[99*len(flows)/100]
	else:
		return 0


parser = argparse.ArgumentParser()
parser.add_argument("-i", "--input", help="input file name")
parser.add_argument("-a","--all",help="print all FCT information",action="store_true")
parser.add_argument("-o","--overall",help="print overall FCT information",action="store_true")
parser.add_argument("-s","--small",help="print FCT information for short flows",action="store_true")
parser.add_argument("-m","--median",help="print FCT information for median flows",action="store_true")
parser.add_argument("-l","--large",help="print FCT information for large flows",action="store_true")
parser.add_argument("--avg",help="only print average FCT information for flows in specific ranges",action="store_true")
parser.add_argument("--tail",help="only print tail (99th percentile) FCT information for flows in specific ranges",action="store_true")

#All the flows
flows=[]
flows_notimeouts=[];	#Flows without timeouts
#Short flows (0,100KB)
short_flows=[]
short_flows_notimeouts=[];
#Large flows (10MB,)
large_flows=[]
large_flows_notimeouts=[];
#Median flows (100KB, 10MB)
median_flows=[]
median_flows_notimeouts=[];
#The number of total timeouts
timeouts=0

args = parser.parse_args()
if args.input:
	fp = open(args.input)
	#Get overall average normalized FCT
	while True:
		line=fp.readline()
		if not line:
			break
		if len(line.split())<3:
			continue
		pkt_size=int(float(line.split()[0]))
		byte_size=float(pkt_size)*1460
		time=float(line.split()[1])
		result=time*1000

		#TCP timeouts
		timeouts_num=int(line.split()[2])
		timeouts+=timeouts_num

		flows.append(result)
		#If the flow is a short flow
		if byte_size<100*1024:
			short_flows.append(result)
		#If the flow is a large flow
		elif byte_size>10*1024*1024:
			large_flows.append(result)
		else:
			median_flows.append(result)

		#If this flow does not have timeout
		if timeouts_num==0:
			flows_notimeouts.append(result)
			#If the flow is a short flow
			if byte_size<100*1024:
				short_flows_notimeouts.append(result)
			#If the flow is a large flow
			elif byte_size>10*1024*1024:
				large_flows_notimeouts.append(result)
			else:
				median_flows_notimeouts.append(result)

	fp.close()

	if args.all:
		print "There are "+str(len(flows))+" flows in total"
		print "There are "+str(timeouts)+" TCP timeouts in total"
		print "Overall average FCT is:\n"+str(avg(flows))
		print "Average FCT for "+str(len(short_flows))+" flows in (0,100KB)\n"+str(avg(short_flows))
		print "99th percentile FCT for "+str(len(short_flows))+" flows in (0,100KB)\n"+str(max(short_flows))
		print "Average FCT for "+str(len(median_flows))+" flows in (100KB,10MB)\n"+str(avg(median_flows))
		print "Average FCT for "+str(len(large_flows))+" flows in (10MB,)\n"+str(avg(large_flows))

	#Overall FCT information
	if args.overall:
		#Only average FCT information
		if args.avg and not args.tail:
			print "Overall average FCT is:\n"+str(avg(flows))
		#Only tail FCT information
		elif not args.avg and args.tail:
			print "Overall 99th percentile FCT is:\n"+str(max(flows))
		else:
			print "Overall average FCT is:\n"+str(avg(flows))
			print "Overall 99th percentile FCT is:\n"+str(max(flows))

	#FCT information for short flows
	if args.small:
		#Only average FCT information
		if args.avg and not args.tail:
			print "Average FCT for flows in (0,100KB) is:\n"+str(avg(short_flows))
		#Only tail FCT information
		elif not args.avg and args.tail:
			print "99th percentile FCT for flows in (0,100KB) is:\n"+str(max(short_flows))
		else:
			print "Average FCT for flows in (0,100KB) is:\n"+str(avg(short_flows))
			print "99th percentile FCT for flows in (0,100KB) is:\n"+str(max(short_flows))

	#FCT information for median flows
	if args.median:
		#Only average FCT information
		if args.avg and not args.tail:
			print "Average FCT for flows in (100KB,10MB) is:\n"+str(avg(median_flows))
		#Only tail FCT information
		elif not args.avg and args.tail:
			print "99th percentile FCT for flows in (100KB,10MB) is:\n"+str(max(median_flows))
		else:
			print "Average FCT for flows in (100KB,10MB) is:\n"+str(avg(median_flows))
			print "99th percentile FCT for flows in (100KB,10MB) is:\n"+str(max(median_flows))

	#FCT information for large flows
	if args.large:
		#Only average FCT information
		if args.avg and not args.tail:
			print "Average FCT for flows in (10MB,) is:\n"+str(avg(large_flows))
		#Only tail FCT information
		elif not args.avg and args.tail:
			print "99th percentile FCT for flows in (10MB,) is:\n"+str(max(large_flows))
		else:
			print "Average FCT for flows in (10MB,) is:\n"+str(avg(large_flows))
			print "99th percentile FCT for flows in (10MB,) is:\n"+str(max(large_flows))


	#print "There are "+str(len(flows_notimeouts))+" flows w/o timeouts in total"
	#print "Overall average FCT w/o timeouts: "+str(avg(flows_notimeouts))
	#print "Average FCT (0,100KB) w/o timeouts: "+str(len(short_flows_notimeouts))+" flows "+str(avg(short_flows_notimeouts))
	#print "99th percentile FCT (0,100KB): "+str(len(short_flows_notimeouts))+" flows "+str(max(short_flows_notimeouts))
	#print "Average FCT (100KB,10MB) w/o timeouts: "+str(len(median_flows_notimeouts))+" flows "+str(avg(median_flows_notimeouts))
	#print "Average FCT (10MB,) w/o timeouts: "+str(len(large_flows_notimeouts))+" flows "+str(avg(large_flows_notimeouts))

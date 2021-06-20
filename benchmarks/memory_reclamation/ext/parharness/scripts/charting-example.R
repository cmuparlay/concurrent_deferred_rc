# Copyright 2015 University of Rochester
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License. 


library(plyr)
library(stringr)
library(utils)
library(ggplot2)
library(scales)
library(grid)


#read.csv("./data//node18x2a-1-22-15.csv")->data
#read.csv("./data//fai.csv")->fai
#read.csv("./data//node18x2a-2-10-15.csv")->data2
#fai$rideable<-"FAI"
#rbind.fill(data,fai)->data
#subset(data,!str_detect(rideable,"GenericDualNB"))->data
#rbind.fill(data,data2)->data

#read.csv("./data//node18x2a-3-10-15.csv")->data
#read.csv("./data//node18x2a-3-10-15-fai.csv")->fai
#rbind(data,read.csv("./data/node18x2a-3-15-15.csv"))->data
#rbind(fai,read.csv("./data/node18x2a-3-15-15-fai.csv"))->fai

read.csv("../../../m6_hashmap_hazard_range_rcu_he_exp.csv")->expdata
read.csv("../../../m6_hashmap_hazard_range_rcu_he.csv")->lindata

#fai$rideable<-"FAI"
#rbind.fill(data,fai)->data

#data$rideable<- lapply(data$rideable, function(x) str_replace(x,"\t",""))
#data$rideable<-as.factor(gsub("\t","",data$rideable))
#data$rideable<-as.factor(gsub("GenericDualNB ","GDC-NB ",data$rideable))
#data$rideable<-as.factor(gsub("GenericDual ","GDC-B ",data$rideable))
#data$rideable<-as.factor(gsub("DQ ","DQ-",data$rideable))
#data$rideable<-as.factor(gsub("Nonblocking","NB",data$rideable))
#data$rideable<-as.factor(gsub("Blocking","B",data$rideable))
#data$rideable<-as.factor(gsub("MHOL","HMOL",data$rideable))
#data$rideable<-as.factor(gsub("FCDualQueue","FC Dual Queue",data$rideable))
#data$rideable<-as.factor(gsub("SSDualQueue","SS Dual Queue",data$rideable))

expdata$rideable<-as.factor(gsub("SortedUnorderedMapRCU","RCU",expdata$rideable))
expdata$rideable<-as.factor(gsub("SortedUnorderedMapHE","HE",expdata$rideable))
expdata$rideable<-as.factor(gsub("SortedUnorderedMapRange","Interval",expdata$rideable))
expdata$rideable<-as.factor(gsub("SortedUnorderedMap","Hazard",expdata$rideable))

lindata$rideable<-as.factor(gsub("SortedUnorderedMapRCU","RCU",lindata$rideable))
lindata$rideable<-as.factor(gsub("SortedUnorderedMapHE","HE",lindata$rideable))
lindata$rideable<-as.factor(gsub("SortedUnorderedMapRange","Interval",lindata$rideable))
lindata$rideable<-as.factor(gsub("SortedUnorderedMap","Hazard",lindata$rideable))

#expdata$rideable<-as.factor(gsub("BonsaiTreeRange","Interval",expdata$rideable))
#expdata$rideable<-as.factor(gsub("BonsaiTreeHazard","Hazard",expdata$rideable))
#lindata$rideable<-as.factor(gsub("BonsaiTreeRange","Interval",lindata$rideable))
#lindata$rideable<-as.factor(gsub("BonsaiTreeHazard","Hazard",lindata$rideable))

ddply(.data=expdata,.(rideable,threads),mutate,ops_max= max(ops)/(interval*1000000))->expdata
ddply(.data=lindata,.(rideable,threads),mutate,ops_max= max(ops)/(interval*1000000))->lindata
#fastdata <- subset(data,str_detect(rideable,"PDQ"))
#gendata <- subset(data,str_detect(rideable,"GD") & rideable!="GDC-B (MSQ:MSQ)" &
#                    rideable!="GDC-NB (LCRQ:HMOL)")
#compdata <- subset(data,str_detect(rideable,"FC Dual") | str_detect(rideable,"SS Dual") )
#lcrqdata <- subset(data,rideable=="LCRQ")
#bestfastdata <- subset(data,str_detect(rideable,"MPDQ-B") | str_detect(rideable,"SPDQ-N"))
#bestgendata <- subset(data,rideable=="GDC-B (LCRQ:TStack)")
#faidata <- subset(data,rideable=="FAI")
hazarddataexp <- subset(expdata,rideable=="Hazard")
rangedataexp <- subset(expdata,rideable=="Interval")
rcudataexp <- subset(expdata,rideable=="RCU")
hedataexp <- subset(expdata,rideable=="HE")

hazarddatalin <- subset(lindata,rideable=="Hazard")
rangedatalin <- subset(lindata,rideable=="Interval")
rcudatalin <- subset(lindata,rideable=="RCU")
hedatalin <- subset(lindata,rideable=="HE")

color_key = c("#73D54C", "#87CECF","#000000","#D24D32", "#474425",
               "#B13EF3","#C38376","#C3C88C","#C58B37",
               "#688CC2","#5B8438","#742C2A","#6CD49B",
               "#507271","#9A64A5","#CD4976","#CE56DC", "#F33E3E","#3366ff",
              "#225533","#F3923E")
#,"#C144A1","#4B3253","#CFD248")
names(color_key) <- unique(c(as.character(expdata$rideable)))
#http://sape.inf.usi.ch/quick-reference/ggplot2/shape
shape_key = c(15,16,17,62,18,19,32,6,5,86,88,111,12,2,60,32,9,3,79,8,11,13,85)
names(shape_key) <- unique(c(as.character(expdata$rideable)))

line_key = c(1,2,3,4,5,6,1,2,3,4,5,6,1,2,3,4,5,6,1,2,3,4,5,1,2,3,4,5,6)
names(line_key) <- unique(c(as.character(expdata$rideable)))



names(color_key) <- unique(c(as.character(lindata$rideable)))
#http://sape.inf.usi.ch/quick-reference/ggplot2/shape
shape_key = c(15,16,17,62,18,19,32,6,5,86,88,111,12,2,60,32,9,3,79,8,11,13,85)
names(shape_key) <- unique(c(as.character(lindata$rideable)))

line_key = c(1,2,3,4,5,6,1,2,3,4,5,6,1,2,3,4,5,6,1,2,3,4,5,1,2,3,4,5,6)
names(line_key) <- unique(c(as.character(lindata$rideable)))


expdata = rbind(hazarddataexp, rangedataexp, rcudataexp, hedataexp)
expchart<-ggplot(data=expdata,
                  aes(x=threads,y=ops_max,color=rideable, shape=rideable, linetype=rideable))+
  geom_line()+xlab("Threads")+ylab("Throughput (M ops/sec)")+geom_point(size=3.5)+
  scale_shape_manual(values=shape_key[names(shape_key) %in% expdata$rideable])+
  scale_linetype_manual(values=line_key[names(line_key) %in% expdata$rideable])+
  theme_bw()+ guides(shape=guide_legend(title=NULL))+ 
  guides(color=guide_legend(title=NULL))+
  guides(linetype=guide_legend(title=NULL))+
  scale_color_manual(values=color_key[names(color_key) %in% expdata$rideable])+
  scale_x_log10(breaks=c(1,2,3,4,5,6,8,10,20,30,40,50,60,70),
                minor_breaks=c(1,2,3,4,5,6,8,10,20,30,40,50,60,70))+
  theme(plot.margin = unit(c(.2,0,.2,0), "cm"))+
  ylim(0,150)
expchart

lindata = rbind(hazarddatalin, rangedatalin, rcudatalin, hedatalin)
linchart<-ggplot(data=lindata,
                  aes(x=threads,y=ops_max,color=rideable, shape=rideable, linetype=rideable))+
  geom_line()+xlab("Threads")+ylab("Throughput (M ops/sec)")+geom_point(size=3.5)+
  scale_shape_manual(values=shape_key[names(shape_key) %in% lindata$rideable])+
  scale_linetype_manual(values=line_key[names(line_key) %in% lindata$rideable])+
  theme_bw()+ guides(shape=guide_legend(title=NULL))+ 
  guides(color=guide_legend(title=NULL))+
  guides(linetype=guide_legend(title=NULL))+
  scale_color_manual(values=color_key[names(color_key) %in% lindata$rideable])+
  scale_x_continuous(breaks=c(1,5,10,15,20,25,30,35,40,45,50,55,60,65,70),
                minor_breaks=c(1,5,10,15,20,25,30,35,40,45,50,55,60,65,70))+
  theme(plot.margin = unit(c(.2,0,.2,0), "cm"))+
  ylim(0,150)
linchart

#gdata = rbind(lcrqdata,gendata,compdata)
#genchart<-ggplot(data=gdata,
#                 aes(x=threads,y=ops_max,color=rideable, shape=rideable, linetype=rideable))+
#  geom_line()+xlab("Threads")+ylab("Throughput (M ops/sec)")+geom_point(size=3.5)+
#  scale_linetype_manual(values=line_key[names(line_key) %in% gdata$rideable])+
#  scale_shape_manual(values=shape_key[names(shape_key) %in% gdata$rideable])+
#  theme_bw()+ guides(shape=guide_legend(title=NULL))+ 
#  guides(color=guide_legend(title=NULL))+
#  guides(linetype=guide_legend(title=NULL))+
#  scale_color_manual(values=color_key[names(color_key) %in% gdata$rideable])+
#  scale_x_log10(breaks=c(1,2,3,4,5,6,8,10,20,30,40,50,60,70),
#                minor_breaks=c(1,2,3,4,5,6,8,10,20,30,40,50,60,70))+
#  theme(plot.margin = unit(c(.2,0,.2,0), "cm"))+
#  ylim(0, 50)
#genchart

#ggsave(filename ="scripts/genchart.eps",genchart,width=8, height = 5.5, units = "in", dpi=300)
#ggsave(filename ="bonsai_expchart.eps",expchart,width=8, height = 5.5, units = "in", dpi=300)
#ggsave(filename ="bonsai_linchart.eps",linchart,width=8, height = 5.5, units = "in", dpi=300)
#ggsave(filename ="bonsai_expchart.pdf",expchart,width=8, height = 5.5, units = "in", dpi=300)
#ggsave(filename ="bonsai_linchart.pdf",linchart,width=8, height = 5.5, units = "in", dpi=300)

ggsave(filename ="hashmap_expchart.eps",expchart,width=8, height = 5.5, units = "in", dpi=300)
ggsave(filename ="hashmap_linchart.eps",linchart,width=8, height = 5.5, units = "in", dpi=300)
ggsave(filename ="hashmap_expchart.pdf",expchart,width=8, height = 5.5, units = "in", dpi=300)
ggsave(filename ="hashmap_linchart.pdf",linchart,width=8, height = 5.5, units = "in", dpi=300)
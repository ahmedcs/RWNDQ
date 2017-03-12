/*
 * RWNDQ - Receive WiNDow based Queue  (RWNDQ) management for TCP congestion Control in data centers.
 *
 *  Author: Ahmed Mohamed Abdelmoniem Sayed, <ahmedcs982@gmail.com, github:ahmedcs>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of CRAPL LICENCE avaliable at
 *    http://matt.might.net/articles/crapl/.
 *    http://matt.might.net/articles/crapl/CRAPL-LICENSE.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the CRAPL LICENSE for more details.
 *
 * Please READ carefully the attached README and LICENCE file with this software
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/netfilter.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/netdevice.h>
#include <linux/inet.h>
#include <net/tcp.h>
#include <net/checksum.h>
#include <linux/netfilter_ipv4.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <net/pkt_sched.h>
#include <linux/openvswitch.h>


#include "datapath.h"

#define DEV_MAX 10

static bool rwndq_enable = false;
module_param(rwndq_enable, bool, 0644);
MODULE_PARM_DESC(rwndq_enable, " rwndq_enable enables RWNDQ on all ports of the vswitch");

static int M = 8;
module_param(M, int, 0);
MODULE_PARM_DESC(M, "M determines number of intervals before updating the window");

static long int interval = 1000L;
module_param(interval, long, 0);
MODULE_PARM_DESC(interval, "interval determines the timer interval in microseconds");

static spinlock_t globalLock;
static struct hrtimer my_hrtimer;
static ktime_t ktime;

static int devcount=0;
static bool timerrun=false;
static unsigned int count=0;
//static char* devname[10];
static int devindex[DEV_MAX];
static int wnd[DEV_MAX];
static int localwnd[DEV_MAX];
static int conncount[DEV_MAX];
static int incr[DEV_MAX];
static unsigned int MSS[DEV_MAX];
static bool slowstart[DEV_MAX];
static bool fail=false;

enum hrtimer_restart timer_callback(struct hrtimer *timer)
{

    unsigned long flags;         	 					//variable for save current states of irq

    timerrun=false;
    struct net_device * dev = first_net_device(&init_net);
    int i=0;
    while (dev!=NULL && i<devcount)
    {

        if(strcmp((const char*)dev->name, "lo") == 0)
        {
            dev = next_net_device(dev);
            continue;
        }

        //if(strcmp((const char*)dev->name, devname[i]) == 0 && conncount[i]>0)
        if(dev->ifindex == devindex[i] && conncount[i]>0)
        {

            timerrun=true;
            if(slowstart[i] && dev->qdisc->qstats.backlog >= ( (dev->qdisc->limit * psched_mtu(dev)) >> 2))
            {
                slowstart[i]=false;
            }
            if(!slowstart[i])
            {
                incr[i] +=( (dev->qdisc->limit * psched_mtu(dev)) >> 2) - dev->qdisc->qstats.backlog;
            }
            else
                incr[i] += 2 * MSS[i];

            if (count == M)
            {
                localwnd[i] += incr[i]/M;
                if(localwnd[i]> 65535 * conncount[i])
                    localwnd[i]=65535 * conncount[i];
                else if(localwnd[i] < conncount[i] * TCP_MIN_MSS)
                    localwnd[i] = TCP_MIN_MSS * conncount[i];
                //spin_lock_irqsave(&globalLock,flags);
                wnd[i] = localwnd[i]/conncount[i];
                //spin_unlock_irqrestore(&globalLock,flags);
                incr[i] = 0;


            }
            i++;
        }
        dev = next_net_device(dev);

    }
    if(count == M)
        count=0;
    else
        count++;

    //runcount++;
    if(timerrun && rwndq_enable)
    {
        //tjnow = jiffies;
        ktime_t ktnow = hrtimer_cb_get_time(&my_hrtimer);
        int overrun = hrtimer_forward(&my_hrtimer, ktnow, ktime);
        //printk(KERN_INFO " testjiffy jiffies %lu ; ret: %d ; ktnsec: %lld \n", tjnow, ret_overrun, ktime_to_ns(kt_now));
        return HRTIMER_RESTART;
    }
    else
        return HRTIMER_NORESTART;

}

void process_packet(struct sk_buff *skb, struct vport *inp , struct vport *outp )
{
	if(!rwndq_enable)
		return;
    const struct net_device *in=netdev_vport_priv(inp)->dev;
    const struct net_device *out=netdev_vport_priv(outp)->dev;

    unsigned long flags;         	 					//variable for save current states of irq

    if (skb && in && out && !fail)
    {

        struct iphdr * ip_header = (struct iphdr *)skb_network_header(skb);
        if (ip_header && ip_header->protocol == IPPROTO_TCP)
        {

            struct tcphdr * tcp_header = (void *)(skb_network_header(skb) + ip_header->ihl * 4);
            int k=0;
            int i=-1;
            int j=-1;
            while(k < devcount)
            {
                if(devindex[k] == in->ifindex)
                    i=k;
                if(devindex[k] == out->ifindex)
                    j=k;
                k++;
            }
            if(i==-1 || j==-1)
            {
                if(i==-1)
                {
                    add_dev_db(in);
                }
                if(j==-1)
                {
                    add_dev_db(out);
                }
            }

            if(tcp_header->syn && tcp_header->ack)
            {
                if(conncount[i]==0)
                {
                    update_dev(in,i);

                }
                if(conncount[j]==0)
                {
                    update_dev(out,j);
                }

                conncount[i]+=1;		//Increase connection numbers
                conncount[j]+=1;		//Increase connection numbers                //
                //printk(KERN_INFO "SYN [%d:%s] openconn -> inconncount = %d, ACK= %d \n", devindex[i], (const char*)in->name, conncount[i], tcp_header->ack);
                //printk(KERN_INFO "SYN [%d:%s] openconn -> inconncount = %d, ACK= %d \n", devindex[j], (const char*)out->name, conncount[j], tcp_header->ack);


                if(conncount[i] >= 2)
                {
                    //spin_lock_irqsave(&globalLock,flags);
                    wnd[i] = wnd[i] * (conncount[i]-1) / conncount[i];
                    //spin_unlock_irqrestore(&globalLock,flags);


                }
                if(conncount[j] >= 2)
                {
                    //spin_lock_irqsave(&globalLock,flags);
                    wnd[j] = wnd[j] * (conncount[j]-1) / conncount[j];
                    //spin_unlock_irqrestore(&globalLock,flags);

                }
                if(!timerrun)
                {

                    if (hrtimer_active(&my_hrtimer) != 0)
                    {
                        hrtimer_cancel(&my_hrtimer);
                    }
                    hrtimer_init(&my_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
                    my_hrtimer.function = &timer_callback;
                    hrtimer_start(&my_hrtimer, ktime, HRTIMER_MODE_REL);
                    timerrun=true;

                }

            }

            if ((tcp_header->fin || tcp_header->rst))
            {


                conncount[i]-=1;
                if(conncount[i]<=0)
                {
                    conncount[i]=0;
                    /*spin_lock_irqsave(&globalLock,flags);
                    update_dev(in,i);
                    spin_lock_irqsave(&globalLock,flags);*/

                }
                else
                {
                    //spin_lock_irqsave(&globalLock,flags);
                    wnd[i]= wnd[i] * (conncount[i]+1) / conncount[i];
                    //spin_unlock_irqrestore(&globalLock,flags);

                }
                printk(KERN_INFO "FIN/RST [%d:%s] openconn -> inconncount = %d, ACK= %d \n", devindex[i], (const char*)in->name, conncount[i], tcp_header->ack);

            }

            if (tcp_header->ack && conncount[i] && (wnd[i] < ntohs(tcp_header->window) ) )
            {

                __be16 old_win = tcp_header->window;
                __be16 new_win = htons(wnd[i]);
                tcp_header->window = new_win;
                csum_replace2(&tcp_header->check, old_win, new_win);
                //printk(KERN_INFO "[%s] new window = %d %d\n", (const char*)in->name, ntohs(old_win) , ntohs(tcp_header->window));


            }


        }
    }
}

void add_dev_db(const struct net_device * dev)
{
    if(dev==NULL || devcount+1>DEV_MAX)
    {
        fail=true;
        timerrun=false;
        printk(KERN_INFO "Fatal Error Exceed Allowed number of Devices : %d \n", devcount);
        return;
    }
    devindex[devcount] = dev->ifindex;
    MSS[devcount] = (psched_mtu(dev) - 54);
    wnd[devcount] = (dev->qdisc->limit * MSS[devcount]) >> 2;
    localwnd[devcount] = (dev->qdisc->limit * MSS[devcount]) >> 2;

    conncount[devcount] = 0;
    incr[devcount] = 0;
    slowstart[devcount]=true;

    printk(KERN_INFO "ADD: [%i:%s] initials : %d %d %d %d %d %d\n", devindex[devcount], (const char*)dev->name ,  dev->qdisc->limit, dev->tx_queue_len, psched_mtu(dev), wnd[devcount],localwnd[devcount], MSS[devcount] );
    devcount++;
    printk(KERN_INFO "ADD: total number of detected devices : %d \n", devcount);

}

void update_dev(const struct net_device * dev, int i)
{
    if(dev==NULL)
        return;
    devindex[i] = dev->ifindex;
    MSS[i] = (psched_mtu(dev) - 54);
    wnd[i] = (dev->qdisc->limit * MSS[i]) >> 2;
    localwnd[i] = (dev->qdisc->limit * MSS[i]) >> 2;
    conncount[i] = 0;
    incr[i] = 0;
    slowstart[i]=true;

    printk(KERN_INFO "update: [%i:%s] initials : %d %d %d %d %d %d\n", devindex[i], (const char*)dev->name ,  dev->qdisc->limit, dev->tx_queue_len, psched_mtu(dev), wnd[i],localwnd[i], MSS[i] );
    return;

}

void del_dev(const struct net_device * dev)
{
    if(dev==NULL || devcount<=0)
        return;
    int i=0;
    while(i<devcount && devindex[i]!=dev->ifindex)
    {
        i++;
    }
    if(i<devcount)
    {
        printk(KERN_INFO "DEL: [%d:%s] \n", devindex[i], (const char*)dev->name);
        int j=i;
        while(j<devcount && devindex[j+1]!=-1)
        {
            devindex[j] = devindex[j+1];
            MSS[j] = MSS[j+1];
            wnd[j] = wnd[j+1];
            localwnd[j] = localwnd[j+1];

            conncount[j] = conncount[j+1];
            incr[j] = incr[j+1];
            slowstart[j]=slowstart[j+1];

            j++;
        }

        devcount--;
        printk(KERN_INFO "DEL: total number of detected devices : %d \n", devcount);
    }
}


void init_rwndq(void)
{

    spin_lock_init(&globalLock);
    timerrun=false;

    devcount=0;
    fail=false;

    int i=0;
    while( i < 10)
    {
        /*strcpy(devname[i], (const char*)dev->name);
        printk(KERN_INFO "[Init] dev number : %d , name: %s\n", i, devname[i]);*/
        devindex[i]=-1;
        conncount[i]=0;
        MSS[i]=0;
        wnd[i]=0;
        localwnd[i]=0;
        incr[i]=0;
        slowstart[i]=false;
        i++;

    }
    if(interval<0)
        interval = 1000L;
    if(M<0)
        M=8;
        
    printk(KERN_INFO "OpenvSwitch Init RWNDQ: interval : %ld , M : %d rwndq_enable : %d\n", interval, M, rwndq_enable);

    printk(KERN_INFO "OpenvSwitch Init RWNDQ: total number of detected devices : %d , timer running : %d\n", devcount, timerrun);

    return;
}

void cleanup_rwndq(void)
{
    int ret_cancel = 0;
    while( hrtimer_callback_running(&my_hrtimer) )
    {
        ret_cancel++;
    }
    if (ret_cancel != 0)
    {
        printk(KERN_INFO " testjiffy Waited for hrtimer callback to finish (%d)\n", ret_cancel);
    }
    if (hrtimer_active(&my_hrtimer) != 0)
    {
        ret_cancel = hrtimer_cancel(&my_hrtimer);
        printk(KERN_INFO " testjiffy active hrtimer cancelled: %d \n", ret_cancel);
    }
    if (hrtimer_is_queued(&my_hrtimer) != 0)
    {
        ret_cancel = hrtimer_cancel(&my_hrtimer);
        printk(KERN_INFO " testjiffy queued hrtimer cancelled: %d \n", ret_cancel);
    }
    printk(KERN_INFO "Stop RWNDQ \n");


}

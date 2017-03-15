# RWNDQ
RWNDQ is a switch-based Equal Share Allocation scheme targeted for Data Centre Networks. The idea is to allow the switch allocate equal share of buffer space to maintain a target queue occupancy for all competeing flows.  

It is implemented as a load-able Linux-Kernel Module and as a Patch applicable to OpenvSwitch datapath module

# Installation Guide
Please Refer to the \[[InstallME](InstallME.md)\] file for more information about installation and possible usage scenarios.

#Feedback
I always welcome and love to have feedback on the program or any possible improvements, please do not hesitate to contact me by commenting on the code or dropping me an email at ahmedcs982@gmail.com. **PS: this is one of the reasons for me to share the software.**  

**This software will be constantly updated as soon as bugs, fixes and/or optimization tricks have been identified.**


# License
This software including (source code, scripts, .., etc) within this repository and its subfolders are licensed under CRAPL license.

**Please refer to the LICENSE file \[[CRAPL LICENCE](LICENSE)\] for more information**


# CopyRight Notice
The Copyright of this repository and its subfolders are held exclusively by "Ahmed Mohamed Abdelmoniem Sayed", for any inquiries contact me at (ahmedcs982@gmail.com).

Any USE or Modification to the (source code, scripts, .., etc) included in this repository has to cite the following PAPERS:  

1- Ahmed M. Abdelmoniemand Brahim Bensaou, “Reconciling Mice and Elephants in Data Center Networks”, in Proceedings of IEEE CloudNet 2015, Vancouver, CA, Oct-2015. 
2- Ahmed M. Abdelmoniem and Brahim Bensaou, “Efficient Switch-Assisted Congestion Control for Data Centers: an Implementation and Evaluation”, in Proceedings of IEEE IPCCC 2015, Nanjing, China, Dec-2015. 

**Notice, the COPYRIGHT and/or Author Information notice at the header of the (source, header and script) files can not be removed or modified.**


# Published Paper
To understand the framework and proposed solution, please read the paper "Reconciling Mice and Elephants in Data Center Networks" \[[RWNDQ Paper1 PDF](download/RWNDQ1.pdf)\] and the longer improved version with implementation details titled "Efficient Switch-Assisted Congestion Control for Data Centers: an Implementation and Evaluation" \[[RWNDQ Paper2 PDF](download/RWNDQ2.pdf)\]

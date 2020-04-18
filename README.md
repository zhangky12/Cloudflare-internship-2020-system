# Cloudflare-internship-2020-system
This is the project for applying cloudflare system internship of 2020

This is a small Ping CLI application for Linux, which has been successfully tested on Ubuntu 14.04.5 LTS (GNU/Linux 4.15.0-1063-aws x86_64). 

## Requirements 
### 1.Use one of the specified languages
This application is written in C, and standard library <netinet/ip_icmp.h> is used.

### 2.Build a tool with a CLI interface
This application accepts hostname/ip address and an optional argument ttl.

```
gcc -o ping ping.c
sudo ./ping www.google.com
```

### 3.Send ICMP "echo requests" in an infinite loop
The infinite while loop in main will keep sending icmp requests.

![echo_requests](https://github.com/zhangky12/Cloudflare-internship-2020-system/blob/master/images/echo_requests.png
)

### 4. Report loss and RTT times for each message
RTT is retrieved from icmp_reply. When program is stopped by ctrl+C, statistics() will be called and print out loss.

![RTT_loss](https://github.com/zhangky12/Cloudflare-internship-2020-system/blob/master/images/RTT_loss.png
)

## Extra Credits
### Allow to set TTL as an argument and report the corresponding "time exceeded” ICMP messages
When the type of icmp_reply is ICMP_TIME_EXCEEDED, "Time to live exceeded" will be printed out.

```
sudo ./ping www.google.com 10
```

![set_RTT](https://github.com/zhangky12/Cloudflare-internship-2020-system/blob/master/images/set_RTT.png
)

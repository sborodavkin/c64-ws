10 open 129, 2, 0, chr$(6) + chr$(0): get#129, a$
20 of = 2: rem rs232 logical device number
30 dim cr$: n = 0: rem client request and number of lines
40 yb = 1: rem 0 - cr, 1 - lf ignoring leading cr
50 d = 1: rem enable debug output
60 gosub 6100
70 if zs$ <> "" then 90
80 goto 60
90 ys$ = zs$: gosub 5000
95 if d = 1 then print "[d] remote says: " + zs$
97 if left$(zs$, 1) = chr$(10) and len(zs$) > 1 then zs$ = mid$(zs$, 2)
100 if zs$ = chr$(13) and n > 0 then 130: rem request complete
110 n = n + 1: cr$(n) = zs$
120 goto 60
130 rem if d = 0 then goto 160
140 print "<<<": for i = 1 to n: print cr$(i): next i: print
150 dim yr$
160 for i = 1 to n: yr$(i) = cr$(i): next i: yn = n
170 gosub 6500: rem parse the request
180 print ">>>": print zr$
190 yr$ = zr$ + ":end:": gosub 6000
200 n = 0: zs$ = "": goto 40

4999 rem ******** subroutines ********
5000 rem ** lowercase petascii string
5010 rem ** in: ys$, string to lowercase
5020 rem ** out: zs$, result string
5030 zs$ = ""
5040 for i = 1 to len(ys$)
5050 tc$ = mid$(ys$, i, 1): ta = asc(tc$)
5060 if ta >= 97 and ta <= 122 then ta = ta - 32
5070 zs$ = zs$ + chr$(ta)
5080 next i
5090 return

6000 rem ** respond with data to rs232
6010 rem ** in: yr$, data to print
6020 rem ** out: none
6030 if d = 0 then 6050 
6040 print "response: " + yr$
6050 poke 154, of: rem set output rs232
6060 print yr$
6070 poke 154, 3: rem set output screen
6080 return

6100 rem ** read zs$ from rs232
6110 rem ** in: yb, line delimiter
6120 rem   (0 - cr, 1 - lf ignoring leading cr)
6130 rem ** out: zs$, one line of input
6140 rem   (valid if not empty.)
6150 rem         zt$, temp accumulator
6160 rem   (don't use externally.)
6170 poke 153, of: rem set input rs232
6180 xc$ = "": get xc$: if xc$ = "" then 6180
6190 if xc$ = chr$(10) and yb = 1 then 6230: rem strip cr if any
6200 if xc$ = chr$(13) and yb = 0 then 6250: rem cut line
6210 if len(zt$) < 255 then zt$ = zt$ + xc$: zs$ = "": rem not complete line
6220 poke 153, 0: return
6230 if right$(zt$, 1) = chr$(13) then zt$ = left$(zt$, len(zt$) - 1)
6240 if zt$ = "" then zt$ = chr$(13)
6250 zs$ = zt$: zt$ = ""
6260 poke 153, 0: return

6300 rem ** find index of yy$ in yx$, starting from yy
6310 rem ** in: yx$ (where to find),
6320 rem        yy$ (what to find)
6330 rem        yy (start index in yx$)
6340 rem ** out: zz or 0 if nothing found
6350 zz = 0: for xx = yy to len(yx$)
6360 if mid$(yx$, xx, len(yy$)) = yy$ then 6380
6370 goto 6390
6380 zz = xx: goto 6400
6390 next xx
6400 for xx = 1 to 1: next xx
6410 return

6500 rem ** handle http request
6510 rem ** in: yr$() (array of request lines),
6520 rem        yn - number of request lines
6530 rem ** out: zr$ - http response.
6540 rem ************ parse first line
6545 tl$ = chr$(13) + chr$(10): rem line break
6550 if yn < 1 then print "[d] badrq: yn<1": goto 6700
6560 tr$ = yr$(1): rem request line
6570 yx$ = tr$: yy$ = " ": yy = 1: gosub 6300
6580 if zz = 0 then print "[d] badrq: no space in " + yx$: goto 6700
6590 tm$ = left$(tr$, zz - 1): rem request method
6600 if tm$ = "get" then 6640
6610 if tm$ = "post" then 6690
6620 if tm$ = "head" then 6690
6630 stop: goto 6700
6640 rem ********** handle get request
6650 yx$ = tr$: yy$ = " ": yy = zz + 1: gosub 6300
6660 if zz = 0 then print "[d] badrq: no 2nd space in " + yx$: goto 6700
6670 tv$ = mid$(tr$, zz + 1): rem http version
6680 if tv$ <> "http/1.1" and tv$<>"http/1.0" then print "[d] badrq: "+tv$: 6700
6690 tc$ = "content-length: 12" + tl$
6695 zr$ = "http/1.1 ok" + tl$ + tc$ + tl$ + "hello world!"
6697 return
6700 zr$ = "http/1.1 400 bad request" + chr$(13): return

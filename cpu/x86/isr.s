.extern isr_exception_stub_func
.extern isr_int_funcs

.macro isr_stub num
isr_stub_\num :
	mov $\num, %rdi
	call isr_exception_stub_func
	iretq
.endm

.macro isr_stub_noerr num
isr_stub_\num :
	mov $\num, %rdi
	call isr_exception_stub_func_noerr
	iretq
.endm

.macro isr_stub_func_caller num
isr_func_caller_\num :
	lea isr_int_funcs, %r14
	mov $\num, %r15
	call *(%r14, %r15, 8)
	iretq
.endm

// CPU exceptions and traps
isr_stub 0
isr_stub 1
isr_stub 2
isr_stub 3
isr_stub 4
isr_stub 5
isr_stub 6
isr_stub 7
isr_stub 8
isr_stub 9
isr_stub 10
isr_stub 11
isr_stub 12
isr_stub 13
isr_stub 14
isr_stub 15
isr_stub 16
isr_stub 17
isr_stub 18
isr_stub 19
isr_stub 20
isr_stub 21
isr_stub 22
isr_stub 23
isr_stub 24
isr_stub 25
isr_stub 26
isr_stub 27
isr_stub 28
isr_stub 29
isr_stub 30
isr_stub 31

// PIC interrupts
isr_stub_noerr 32
isr_stub_noerr 33
isr_stub_noerr 34
isr_stub_noerr 35
isr_stub_noerr 36
isr_stub_noerr 37
isr_stub_noerr 38
isr_stub_noerr 39
isr_stub_noerr 40
isr_stub_noerr 41
isr_stub_noerr 42
isr_stub_noerr 43
isr_stub_noerr 44
isr_stub_noerr 45
isr_stub_noerr 46
isr_stub_noerr 47

.global isr_stub_table
isr_stub_table:
.irp num 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
	.quad isr_stub_\num
.endr

// Unoccupied interrupt vectors
.global isr_int_funcs
isr_int_funcs:
.rept 209
.quad 0
.endr

isr_stub_func_caller 0
isr_stub_func_caller 1
isr_stub_func_caller 2
isr_stub_func_caller 3
isr_stub_func_caller 4
isr_stub_func_caller 5
isr_stub_func_caller 6
isr_stub_func_caller 7
isr_stub_func_caller 8
isr_stub_func_caller 9
isr_stub_func_caller 10
isr_stub_func_caller 11
isr_stub_func_caller 12
isr_stub_func_caller 13
isr_stub_func_caller 14
isr_stub_func_caller 15
isr_stub_func_caller 16
isr_stub_func_caller 17
isr_stub_func_caller 18
isr_stub_func_caller 19
isr_stub_func_caller 20
isr_stub_func_caller 21
isr_stub_func_caller 22
isr_stub_func_caller 23
isr_stub_func_caller 24
isr_stub_func_caller 25
isr_stub_func_caller 26
isr_stub_func_caller 27
isr_stub_func_caller 28
isr_stub_func_caller 29
isr_stub_func_caller 30
isr_stub_func_caller 31
isr_stub_func_caller 32
isr_stub_func_caller 33
isr_stub_func_caller 34
isr_stub_func_caller 35
isr_stub_func_caller 36
isr_stub_func_caller 37
isr_stub_func_caller 38
isr_stub_func_caller 39
isr_stub_func_caller 40
isr_stub_func_caller 41
isr_stub_func_caller 42
isr_stub_func_caller 43
isr_stub_func_caller 44
isr_stub_func_caller 45
isr_stub_func_caller 46
isr_stub_func_caller 47
isr_stub_func_caller 48
isr_stub_func_caller 49
isr_stub_func_caller 50
isr_stub_func_caller 51
isr_stub_func_caller 52
isr_stub_func_caller 53
isr_stub_func_caller 54
isr_stub_func_caller 55
isr_stub_func_caller 56
isr_stub_func_caller 57
isr_stub_func_caller 58
isr_stub_func_caller 59
isr_stub_func_caller 60
isr_stub_func_caller 61
isr_stub_func_caller 62
isr_stub_func_caller 63
isr_stub_func_caller 64
isr_stub_func_caller 65
isr_stub_func_caller 66
isr_stub_func_caller 67
isr_stub_func_caller 68
isr_stub_func_caller 69
isr_stub_func_caller 70
isr_stub_func_caller 71
isr_stub_func_caller 72
isr_stub_func_caller 73
isr_stub_func_caller 74
isr_stub_func_caller 75
isr_stub_func_caller 76
isr_stub_func_caller 77
isr_stub_func_caller 78
isr_stub_func_caller 79
isr_stub_func_caller 80
isr_stub_func_caller 81
isr_stub_func_caller 82
isr_stub_func_caller 83
isr_stub_func_caller 84
isr_stub_func_caller 85
isr_stub_func_caller 86
isr_stub_func_caller 87
isr_stub_func_caller 88
isr_stub_func_caller 89
isr_stub_func_caller 90
isr_stub_func_caller 91
isr_stub_func_caller 92
isr_stub_func_caller 93
isr_stub_func_caller 94
isr_stub_func_caller 95
isr_stub_func_caller 96
isr_stub_func_caller 97
isr_stub_func_caller 98
isr_stub_func_caller 99
isr_stub_func_caller 100
isr_stub_func_caller 101
isr_stub_func_caller 102
isr_stub_func_caller 103
isr_stub_func_caller 104
isr_stub_func_caller 105
isr_stub_func_caller 106
isr_stub_func_caller 107
isr_stub_func_caller 108
isr_stub_func_caller 109
isr_stub_func_caller 110
isr_stub_func_caller 111
isr_stub_func_caller 112
isr_stub_func_caller 113
isr_stub_func_caller 114
isr_stub_func_caller 115
isr_stub_func_caller 116
isr_stub_func_caller 117
isr_stub_func_caller 118
isr_stub_func_caller 119
isr_stub_func_caller 120
isr_stub_func_caller 121
isr_stub_func_caller 122
isr_stub_func_caller 123
isr_stub_func_caller 124
isr_stub_func_caller 125
isr_stub_func_caller 126
isr_stub_func_caller 127
isr_stub_func_caller 128
isr_stub_func_caller 129
isr_stub_func_caller 130
isr_stub_func_caller 131
isr_stub_func_caller 132
isr_stub_func_caller 133
isr_stub_func_caller 134
isr_stub_func_caller 135
isr_stub_func_caller 136
isr_stub_func_caller 137
isr_stub_func_caller 138
isr_stub_func_caller 139
isr_stub_func_caller 140
isr_stub_func_caller 141
isr_stub_func_caller 142
isr_stub_func_caller 143
isr_stub_func_caller 144
isr_stub_func_caller 145
isr_stub_func_caller 146
isr_stub_func_caller 147
isr_stub_func_caller 148
isr_stub_func_caller 149
isr_stub_func_caller 150
isr_stub_func_caller 151
isr_stub_func_caller 152
isr_stub_func_caller 153
isr_stub_func_caller 154
isr_stub_func_caller 155
isr_stub_func_caller 156
isr_stub_func_caller 157
isr_stub_func_caller 158
isr_stub_func_caller 159
isr_stub_func_caller 160
isr_stub_func_caller 161
isr_stub_func_caller 162
isr_stub_func_caller 163
isr_stub_func_caller 164
isr_stub_func_caller 165
isr_stub_func_caller 166
isr_stub_func_caller 167
isr_stub_func_caller 168
isr_stub_func_caller 169
isr_stub_func_caller 170
isr_stub_func_caller 171
isr_stub_func_caller 172
isr_stub_func_caller 173
isr_stub_func_caller 174
isr_stub_func_caller 175
isr_stub_func_caller 176
isr_stub_func_caller 177
isr_stub_func_caller 178
isr_stub_func_caller 179
isr_stub_func_caller 180
isr_stub_func_caller 181
isr_stub_func_caller 182
isr_stub_func_caller 183
isr_stub_func_caller 184
isr_stub_func_caller 185
isr_stub_func_caller 186
isr_stub_func_caller 187
isr_stub_func_caller 188
isr_stub_func_caller 189
isr_stub_func_caller 190
isr_stub_func_caller 191
isr_stub_func_caller 192
isr_stub_func_caller 193
isr_stub_func_caller 194
isr_stub_func_caller 195
isr_stub_func_caller 196
isr_stub_func_caller 197
isr_stub_func_caller 198
isr_stub_func_caller 199
isr_stub_func_caller 200
isr_stub_func_caller 201
isr_stub_func_caller 202
isr_stub_func_caller 203
isr_stub_func_caller 204
isr_stub_func_caller 205
isr_stub_func_caller 206
isr_stub_func_caller 207
isr_stub_func_caller 208

.global isr_func_caller_table
isr_func_caller_table:
.irp num 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208
	.quad isr_func_caller_\num
.endr

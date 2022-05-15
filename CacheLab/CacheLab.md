# **CacheLab Report** By 2019201408陈志朋

### Part - A

##### 1）对指令的处理

根据文档提示，$I$指令可以无需进行处理；$S$和$L$的处理类似，会对cache进行一次查询，如果没有命中的话会将所需的内容加载到cache中；$M$指令可以分解为$S$指令加上$L$指令。同时，文档中限定了每次读取了字节不会分布在两个块中，所以读取的字节长度也不需要进行处理。

代码如下：

```c
void solve() {
    FILE *F = freopen (file, "r", stdin); /* 打开所需文件 */
    
    init_cache();
    char inst[5], addr[100];
    long long tag;
    int l, r, sid;
    while (scanf ("%s", inst) != EOF) { /* 读入指令 */
        scanf ("%s", addr); /* 读入地址 */
        step += 1; /* 更新时间戳 */
        for (l = 0, r = 0; addr[r] != ','; ++r); 
        	/* 得到有效的地址在字符串的起始位置和终止位置 */
        htob(addr, l, r, &tag, &sid); /* 将16进制的地址转化为二进制 */
        
        if (inst[0] == 'I') continue; /* 不对I指令进行处理 */
        else if (inst[0] == 'L') { /* 处理L指令 */
            if (flag) printf ("%s %s", inst, addr); /* 处理-v参数 */
            load_memory(tag, sid); /* 进行一次内存加载 */
            if (flag) puts(""); /* 处理-v参数 */
        }
        else if (inst[0] == 'S') { /* 处理S指令 */
            if (flag) printf ("%s %s", inst, addr); /* 处理-v参数 */
            store_memory(tag, sid); /* 进行一次内存存储 */
            if (flag) puts(""); /* 处理-v参数 */
        }
        else if (inst[0] == 'M') { /* 处理M指令 */
            if (flag) printf ("%s %s", inst, addr); /* 处理-v参数 */
            load_memory(tag, sid); /* 进行一次内存加载 */
            store_memory(tag, sid); /* 进行一次内存储存 */
            if (flag) puts(""); /* 处理-v参数 */
        }
    }

    fclose (F); /* 关闭文件 */
}
```

##### 2）对cache的模拟

可以使用一个$2^s\times E$的二维数组实现。由于$s$和$E$是变量，所以不能使用二维数组，会缺乏可拓展性。这里使用指针的指针来代替二维数组，实现了一个大小可变的二维数组，只需每次使用malloc函数申请内存即可。在模拟cache的同时，需要开一个相同大小的数组，记录cache中每个位置的时间戳。

cache初始化的代码如下：

```c
long long **cache; /* 储存每个位置的tag */
long long **tim; /* 储存对应位置的时间戳 */

void init_cache() {
    cache = malloc((1 << s) * sizeof(long long *)); /* 先开一个行数组 */
    for (int i = 0; i < (1 << s); ++i) {
        cache[i] = malloc(E * sizeof(long long));
    } /* 对每一行开相应的列 */
    for (int i = 0; i < (1 << s); ++i) {
        for (int j = 0; j < E; ++j)
            cache[i][j] = 0;
    } /* 将整个cache数组清零 */

    tim = malloc((1 << s) * sizeof(long long *)); /* 先开一个行数组 */
    for (int i = 0; i < (1 << s); ++i) {
        tim[i] = malloc(E * sizeof(long long));
    } /* 对每一行开相应的列 */
    for (int i = 0; i < (1 << s); ++i) {
        for (int j = 0; j < E; ++j)
            tim[i][j] = 0;
    } /* 将整个cache数组清零 */
}
```

##### 3）处理地址

对于一个地址，由于每个十六进制位对应了$4$个二进制位，所以从最低位开始，用if-else语句实现十六进制到二进制的转化。转化为二进制后，第$b$位至第$(b+s-1)$位对应的是行号，剩下的是tag，这里直接将tag储存为十进制进行储存。

地址转化的代码如下：

```c
void htob(char *addr, int l, int r, long long *tag, int *sid) {
    (*tag) = 0; (*sid) = 0;
    for (int i = 0; i < 64; ++i) bin[i] = 0;
    for (int i = r - 1, p = 0; i >= l; --i) {
        if (addr[i] == '1' || addr[i] == '3' || addr[i] == '5' || addr[i] == '7' ||
            addr[i] == '9' || addr[i] == 'b' || addr[i] == 'd' || addr[i] == 'f')
            bin[p] = 1; /* 处理第1位 */
        p += 1;

        if (addr[i] == '2' || addr[i] == '3' || addr[i] == '6' || addr[i] == '7' ||
            addr[i] == 'a' || addr[i] == 'b' || addr[i] == 'e' || addr[i] == 'f') 
            bin[p] = 1; /* 处理第2位 */
        p += 1;

        if (addr[i] == '4' || addr[i] == '5' || addr[i] == '6' || addr[i] == '7' ||
            addr[i] == 'c' || addr[i] == 'd' || addr[i] == 'e' || addr[i] == 'f') 
            bin[p] = 1; /* 处理第3位 */
        p += 1;

        if (addr[i] == '8' || addr[i] == '9' || addr[i] == 'a' || addr[i] == 'b' ||
            addr[i] == 'c' || addr[i] == 'd' || addr[i] == 'e' || addr[i] == 'f') 
            bin[p] = 1; /* 处理第4位 */
        p += 1;
    }

    for (int i = b + s - 1; i >= b; --i)
        (*sid) =  (*sid) * 2 + bin[i]; /* 计算该地址对应的行号 */
    
    for (int i = 63; i >= b + s; --i)
        (*tag) = (*tag) * 2 + bin[i]; /* 计算该地址对应的tag */
}
```

##### 4）加载内存

对cache查询时，先找到地址对应的行，然后枚举这行中的每一个格子。如果是空格，就进行标记；如果是当前地址的tag，那么该地址已经储存在cache中，退出查找。否则，记录一个时间戳最小的格子。当所有格子都枚举过之后，如果有找到，则为hit。否则为miss，若没有空格子，需要在加上eviction。最后更新格子的tag值和时间戳。

加载内存的代码如下：

```c
void load_memory(long long tag, int sid) {
    int pos = 0;
    bool isfree = false, isfind = false;
    for (int i = 0; i < E; ++i) {
        if (cache[sid][i] == tag && tim[sid][i] != 0) {
            isfind = true;
            tim[sid][i] = step;
            break;
        } /* 当前地址可以在cache中找到 */
        if (tim[sid][i] == 0) isfree = true; /* 当前格是空格 */
        if (tim[sid][i] < tim[sid][pos]) pos = i; /* 找一个时间戳最小的，空格时间戳为0 */
    }
    if (isfind == true) {
        hit += 1;
        if (flag) printf (" hit");
    } /* cache命中 */
    else {
        miss += 1;
        if (flag) printf (" miss"); /* 不命中 */
        if (!isfree) {
            evic += 1;
            printf (" eviction");
        } /* 需要更新cache */
        cache[sid][pos] = tag;
        tim[sid][pos] = step; /* 对cache进行更新 */
    }
}
```

##### 5）内存储存

直接调用加载内存的函数即可。

##### 6）处理命令行参数

使用argc和argv将命令行参数传入到程序中，枚举每一个参数进行处理。

代码如下：

```c
int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) { /* 枚举每一个参数 */
        if (argv[i][1] == 'v') { /* 处理-v */
            flag = true;
        }
        else if (argv[i][1] == 's') { /* 读取s的大小 */
            s = atoi(argv[i + 1]);
            i += 1;
        }
        else if (argv[i][1] == 'E') { /* 读取E的大小 */
            E = atoi(argv[i + 1]);
            i += 1;
        }
        else if (argv[i][1] == 'b') { /* 读取b的大小 */
            b = atoi(argv[i + 1]);
            i += 1;
        }
        else if (argv[i][1] == 't') { /* 读取输入文件的位置 */
            file = argv[i + 1];
            i += 1;
        }
    }
    solve(); /* 调用处理函数 */
    printSummary(hit, miss, evic); /* 输出结果 */
    return 0;
}
```

### Part - B

##### 1）$32 \times 32$的矩阵

由于cache的大小是$s=5$，$E=1$，$b=5$，即一共有$2^5=32$行，每行可以储存一个tag，可以储存$32$个字节的内容。因此，矩阵每$8$行在cache中对应的行数是一样的（第$i$行和第$i+8$行储存在cache中对应的行数）。所以，可以对每$8\times 8$的子矩阵进行分块以最大利用cache。

代码如下：

```c
void transpose1(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, p, q;
    for (i = 0; i < 32; i += 8) {
        for (j = 0; j < 32; j += 8) {
                for (p = i; p < i + 8; ++p)
                    for (q = j; q < j + 8; ++q)
                        B[q][p] = A[p][q];
        }
    }
}
```

测试结果如下图所示：

<img src="C:\Users\user\AppData\Roaming\Typora\typora-user-images\image-20201212210716558.png" alt="image-20201212210716558" style="zoom:80%;" />

但是miss的次数为343次，比最优解还多一点。所以考虑在哪些地方可以降低miss次数。根据cache的大小可以发现，$A$数组对应行在cache中的位置和$B$数组相同行的位置一样。对于处理下图标黄块时，会发生多次的miss。

<img src="C:\Users\user\AppData\Roaming\Typora\typora-user-images\image-20201212211721259.png" alt="image-20201212211721259" style="zoom:80%;" />

可以考虑先处理对角线的块，将上图$A$数组中每行的黄块先移动到$B$数组同行的蓝块上。在将$B$数组的蓝块移动到$B$数组每行对应的黄块上，这样减少了大量的miss。

代码如下：

```c
void transpose1(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, p, q;
    for (i = 0; i < 32; i += 8) {

        for (p = i; p < i + 8; ++p)
            for (q = i; q < i + 8; ++q)
                B[q][(p + 8) % N] = A[p][q]; /* 16次miss */
        
        for (p = i; p < i + 8; ++p)
            for (q = i; q < i + 8; ++q)
                B[p][q] = B[p][(q + 8) % N]; /* 8次miss */
    }

    for (i = 0; i < 32; i += 8) {
        for (j = 0; j < 32; j += 8) {
            if (i != j) {
                for (p = i; p < i + 8; ++p)
                    for (q = j; q < j + 8; ++q)
                        B[q][p] = A[p][q]; /* 16次miss */
            }
            else continue;
        }
    }
}
```

测试结果如下图所示：

<img src="C:\Users\user\AppData\Roaming\Typora\typora-user-images\image-20201212212244328.png" alt="image-20201212212244328" style="zoom:80%;" />

##### 2）$64 \times 64$的矩阵

先尝试$8 \times 8$分块的程序，与上一问的程序类似，测试结果如下图所示，

<img src="C:\Users\user\AppData\Roaming\Typora\typora-user-images\image-20201212214226322.png" alt="image-20201212214226322" style="zoom:80%;" />

发现该算法的测试结果极差。考虑到矩阵大小变大的，所以每$4$行后在cache中对应的行会重复出现一次。所以每个$8 \times 8$分块中的前$4$行和后$4$行会产生冲突，导致了miss次数变多。因此，尝试修改乘$4 \times 4$分块。测试结果如下图所示，

<img src="C:\Users\user\AppData\Roaming\Typora\typora-user-images\image-20201212214541721.png" alt="image-20201212214541721" style="zoom:80%;" />

可以发现，$4 \times 4$分块已经极大的减少了cache冲突的问题，但是由于每块的大小太小，没有充分利用cache的空间，而且块数过多，导致miss的次数仍然很多。

考虑先将整个矩阵按$8 \times 8$分块，每个分块中将前$4$行和后$4$行分开处理。

**第一步**把$A$数组的前$4$行的前$4$个元素和后$4$个元素分别转置，此处会产生$8$次miss，如下图所示（上面的为$A$，下面的为 $B$）：

<img src="C:\Users\user\AppData\Roaming\Typora\typora-user-images\image-20201212215101263.png" alt="image-20201212215101263" style="zoom:80%;" />

注意处理过程中，为了避免cache冲突，可以开$8$个int型的临时变量来储存$A$的某一行，可以避免$A$和$B$两个数组之间的冲突。

**第二步**把$B$的右上角的数拷贝到左下角，然后再将$A$的左下角拷贝到$B$的右上角。在这个过程中，可以通过调整赋值顺序利用已经储存到cache中的数据进行操作：先拷贝$B$数组的右上角到临时变量（4次miss），然后拷贝$A$的左下角到临时变量（4次miss），接着赋值$B$的右上角（已经加载在cache中），最后赋值$B$的右下角（4次miss）。

**第三步**处理$B$的右下角。由于第二步中已经加载了$A$的后四行和$B$的后四行，所以此处不会产生miss，直接处理即可（使用临时变量防止$A$、$B$之间的冲突）。

代码如下：

```c
void transpose2(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, p;
    int a1, a2, a3, a4, a5, a6, a7, a0;

    for (i = 0; i < 64; i += 8) {
        for (j = 0; j < 64; j += 8) {
            for (p = 0; p < 4; ++p) {
                a0 = A[i + p][j + 0];
                a1 = A[i + p][j + 1];
                a2 = A[i + p][j + 2];
                a3 = A[i + p][j + 3];
                a4 = A[i + p][j + 4];
                a5 = A[i + p][j + 5];
                a6 = A[i + p][j + 6];
                a7 = A[i + p][j + 7];

                B[j + 0][i + p] = a0;
                B[j + 1][i + p] = a1;
                B[j + 2][i + p] = a2;
                B[j + 3][i + p] = a3;
                B[j + 0][i + p + 4] = a4;
                B[j + 1][i + p + 4] = a5;
                B[j + 2][i + p + 4] = a6;
                B[j + 3][i + p + 4] = a7;
            }

            for (p = 0; p < 4; ++p) {
                a0 = B[j + p][i + 4];
                a1 = B[j + p][i + 5];
                a2 = B[j + p][i + 6];
                a3 = B[j + p][i + 7];

                a4 = A[i + 4][j + p];
                a5 = A[i + 5][j + p];
                a6 = A[i + 6][j + p];
                a7 = A[i + 7][j + p];


                B[j + p][i + 4] = a4;
                B[j + p][i + 5] = a5;
                B[j + p][i + 6] = a6;
                B[j + p][i + 7] = a7;
                
                B[j + p + 4][i + 0] = a0;
                B[j + p + 4][i + 1] = a1;
                B[j + p + 4][i + 2] = a2;
                B[j + p + 4][i + 3] = a3;
            }

            for (p = 4; p < 8; ++p) {
                a0 = A[i + p][j + 4];
                a1 = A[i + p][j + 5];
                a2 = A[i + p][j + 6];
                a3 = A[i + p][j + 7];

                B[j + 4][i + p] = a0;
                B[j + 5][i + p] = a1;
                B[j + 6][i + p] = a2;
                B[j + 7][i + p] = a3;
            }
        }
    }

}
```

测试结果如下图所示，

<img src="C:\Users\user\AppData\Roaming\Typora\typora-user-images\image-20201212220124514.png" alt="image-20201212220124514" style="zoom:80%;" />

##### 3）$67 \times 61$的矩阵

由于行和列都不是$32$的倍数，所以没有太优秀的分块做法。任何分块做法都会在余数的地方造成大量的冲突，所以先尝试普通的$8 \times 8$分块做法。代码和$32\times 32$的第一种相同。

测试结果如下图所示，

<img src="C:\Users\user\AppData\Roaming\Typora\typora-user-images\image-20201212213418016.png" alt="image-20201212213418016" style="zoom:80%;" />

可以发现结果和要求差一点，所以可以尝试修改分块的大小。当分块的大小变大时，每一次可能会多一些冲突，但是总的运行次数变少了。经过测试，当分块大小为$18\times 18$时，此时测试结果能得到满分。

代码如下，

```c
const int maxn = 18;
void transpose3(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, p, q;

    for (i = 0; i < 67; i += maxn) {
        for (j = 0; j < 61; j += maxn) {
            for (p = i; p < 67 && p < i + maxn; ++p)
                for (q = j; q < 61 && q < j + maxn; ++q) {
                    B[q][p] = A[p][q];
                }
        }
    }
}
```

测试结果如下图所示，

<img src="C:\Users\user\AppData\Roaming\Typora\typora-user-images\image-20201212213646624.png" alt="image-20201212213646624" style="zoom:80%;" />

### 总结

**所有部分均通过测试：**

<img src="C:\Users\user\AppData\Roaming\Typora\typora-user-images\image-20201212213816473.png" alt="image-20201212213816473" style="zoom:80%;" />
# FSLab

### 数据结构

##### 1）SuperBlock

在SuperBlock中，使用了$3$个unsigned int类型变量，分别储存根目录所在的inode、空闲的inode个数、空闲的block个数（此处空闲的block不包括用于储存inode的block）。SuperBlock储存在编号为$0$的block中。

代码如下所示：

```c++
struct SUPERBLOCK {
	unsigned int numInodes;
	unsigned int numDateBlocks;
	unsigned int idRootInode;
};
```

##### 2）Inode

在inode中，储存了模式（文件或文件夹）、大小、创建时间、访问时间、修改时间、该inode指向的block的数量（不包括间接指针指向的block）、$12$个直接指针、$2$个间接指针。由于这里只需要储存对应的block的编号，所以这里的指针为unsigned int类型变量即可。

使用宏定义计算一个block可以存放的inode个数。每个block的前$8$个字节共$64$位储存了这个block中的第$i$个位置是否已经存放了inode。存放inode的block是第$3\sim799$个block。

代码如下所示：

```c++
struct INODE {
	mode_t mode;
	off_t size;
	time_t AccessTime;
	time_t CreateTime;
	time_t ModifiedTime;
	unsigned int numBlocks;
	unsigned int DirectPointer[12];
	unsigned int IndirectPointer[2];
};
```

##### 3）Block

如果这个Block存放的是文件名，即对应的inode是**文件夹**的inode，则该block前$8$个字节记录每个位置是否有储存文件名，文件名的结构体如下所示，最长支持128位的文件名。其中，filename为文件名，position为该文件的inode。

```c++
struct FILENAME {
	char filename[MaxFileName];
	unsigned int position;
};
```

如果这个block存放的是指针，即它是通过间接指针到达的，那么它存放$\frac{4096B}{4B}=1024$个指针，一个位置为$0$表示这个位置没有储存指针，否则该位置的指向对应编号的block。

##### 4）Bitmap

Bitmap存放在第$2\sim3$个block中，用于检测第$i$个block是否被占用。

代码如下：

```c++
unsigned int bitmap[maxBitmap]; // First: 0~32767; Second: 32768~65535
```

### 函数实现

##### fs_getattr

通过文件路径找到这个文件对应的inode。先通过SuperBlock找到根目录的inode，然后访问目录的block找下一个文件的inode，依次类推。直接指针和间接指针需要分开处理，间接指针需要先访问对应的block，然后再读取其中的指针去访问block。

如果没有找到对应的文件，返回错误信息。否则，使用找到的inode中储存的信息去填写struct stat的内容。

##### fs_readdir

先找到对应的文件夹的inode，然后查找它的指针获取该文件夹下的所有文件（文件夹）的名称，使用filler将其写道buffer中。

##### fs_read

需要先计算起点和终点所在的block，然后枚举每一个block。如果这个block不是需要的block，将其跳过。如果这个block与起点所在的block相同，需要使用起点的偏移量作为读取的起点，否则可以使用block的起点作为读取的起点。如果这个block与终点所在的block相同，需要判断文件末尾、block末尾、终点偏移量这三者之间的关系。

获取到相应的数据后，使用memcpy拷贝到buffer上。

##### fs_mknod

先将路径进行切割处理，取出最后一个斜杠之间的路径为父亲路径，最后的为需要新建的文件的名字。找到父亲的inode，然后在已经有的block中寻找一个空的位置，来存放新建的文件。如果找不到，新取一块空闲的block（如果没有则返回错误信息），然后将新建文件的inode的编号和名字（即结构体FILENAME），储存到这个block的位置上，使用disk_write将修改写入磁盘。

##### fs_mkdir

创建文件夹与创建文件同理，只需要在新建的inode中讲模式修改为文件夹模式（S_IFDIR）即可。

##### fs_rmdir

先处理出父路径，然后在父路径的inode指向的block中找对应的文件夹的名字，然后从父路径的inode指向的block中将这个位置标记为未占用。然后把这个inode标记为未占用。

##### fs_unlink

由于保证了文件夹下为空，所以删除文件与删除文件夹类似，同时需要释放占用的block，在Bitmap中将这些block标记为未占用。

##### fs_rename

该操作相当于rmdir（unlink）和mkdir（mknod）二者的组合。不同之处在于删除之后不用释放这个inode对应的空间及block，只需要在新的位置加入这个inode即可。

##### fs_truncate

先找到文件对应的inode，然后判断文件的大小与新的大小的关系。如果比新的size大，则向后面加入新的block。

##### fs_write

与fs_read类似。不同之处在于，fs_read将数据从block中读到参数buffer中，而fs_write将数据从buffer写入到对应的block中。同时，在fs_write开始的地方，需要调用fs_truncate开辟足够的内存空间。对于O_APPEND标记符，手动将offset设置到文件末尾（即inode.size）的地方。

##### fs_utime

找到对应文件的inode，然后修改这个文件的时间信息。

##### fs_statfs

使用SuperBlock的信息进行修改即可。

**此外，fs_open、fs_release、fs_opendir、fs_releasedir这四个函数没有进行修改。**

### 运行结果

##### 0）$0.sh$

<img src="C:\Users\Zhipeng Chen\AppData\Roaming\Typora\typora-user-images\image-20210630155257853.png" alt="image-20210630155257853" style="zoom:67%;" />

##### 1）$1.sh$

<img src="C:\Users\Zhipeng Chen\AppData\Roaming\Typora\typora-user-images\image-20210630155054648.png" alt="image-20210630155054648" style="zoom:67%;" />

##### 2）$2.sh$

<img src="C:\Users\Zhipeng Chen\AppData\Roaming\Typora\typora-user-images\image-20210630155108823.png" alt="image-20210630155108823" style="zoom:67%;" />

##### 3）$3.sh$

<img src="C:\Users\Zhipeng Chen\AppData\Roaming\Typora\typora-user-images\image-20210630155119788.png" alt="image-20210630155119788" style="zoom:67%;" />

##### 4）$4.sh$

<img src="C:\Users\Zhipeng Chen\AppData\Roaming\Typora\typora-user-images\image-20210630155127644.png" alt="image-20210630155127644" style="zoom:67%;" />

##### 5）$5.sh$

<img src="C:\Users\Zhipeng Chen\AppData\Roaming\Typora\typora-user-images\image-20210630155136148.png" alt="image-20210630155136148" style="zoom:67%;" />

##### 6）$6.sh$

<img src="C:\Users\Zhipeng Chen\AppData\Roaming\Typora\typora-user-images\image-20210630155145624.png" alt="image-20210630155145624" style="zoom:67%;" />

##### 7）$7.sh$

<img src="C:\Users\Zhipeng Chen\AppData\Roaming\Typora\typora-user-images\image-20210630155200129.png" alt="image-20210630155200129" style="zoom:67%;" />

##### 8）$8.sh$

<img src="C:\Users\Zhipeng Chen\AppData\Roaming\Typora\typora-user-images\image-20210630155207072.png" alt="image-20210630155207072" style="zoom:67%;" />

##### 9）$9.sh$

该测试点由于奇怪的原因，即使是在服务器的文件系统上（没有使用我写的文件系统），仍然无法运行df相关的指令，故该测试点无法进行测试。

##### 10）$10.sh$

<img src="C:\Users\Zhipeng Chen\AppData\Roaming\Typora\typora-user-images\image-20210630155248180.png" alt="image-20210630155248180" style="zoom:67%;" />

##### 11）$11.sh$

<img src="C:\Users\Zhipeng Chen\AppData\Roaming\Typora\typora-user-images\image-20210630155341369.png" alt="image-20210630155341369" style="zoom:67%;" />

##### 12）$12.sh$

<img src="C:\Users\Zhipeng Chen\AppData\Roaming\Typora\typora-user-images\image-20210630155334147.png" alt="image-20210630155334147" style="zoom:67%;" />

##### 13）$13.sh$

<img src="C:\Users\Zhipeng Chen\AppData\Roaming\Typora\typora-user-images\image-20210630155600146.png" alt="image-20210630155600146" style="zoom:67%;" />

##### 14）$14.sh$

该测试点的输出过多，因此截图只截部分。

![image-20210630155352380](C:\Users\Zhipeng Chen\AppData\Roaming\Typora\typora-user-images\image-20210630155352380.png)

##### 15）$15.sh$

<img src="C:\Users\Zhipeng Chen\AppData\Roaming\Typora\typora-user-images\image-20210630155241555.png" alt="image-20210630155241555" style="zoom:67%;" />


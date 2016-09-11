#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <linux/videodev2.h>
#include <unistd.h>
#include <jpeglib.h>

struct buffer
{
    void *start;
    size_t length;
};

struct vdIn
{
    int width;
    int height;
    unsigned char *framebuffer;
}vd;

struct buffer *buffers = NULL;

/******************************************************************************
Description.: yuv2jpeg function is based on compress_yuyv_to_jpeg written by
              Gabriel A. Devenyi.
              It uses the destination manager implemented above to compress
              YUYV data to JPEG. Most other implementations use the
              "jpeg_stdio_dest" from libjpeg, which can not store compressed
              pictures to memory instead of a file.
Input Value.: video structure from v4l2uvc.c/h, destination buffer and buffersize
              the buffer must be large enough, no error/size checking is done!
Return Value: the buffer will contain the compressed data
******************************************************************************/
int compress_yuyv_to_jpeg(struct vdIn *vd, unsigned char **buffer, unsigned long *len, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    unsigned char *line_buffer, *yuyv;
    int z;

    line_buffer = calloc(vd->width * 3, 1);
    yuyv = vd->framebuffer;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    /* jpeg_stdio_dest (&cinfo, file); */
    jpeg_mem_dest(&cinfo, buffer, len);

    cinfo.image_width = vd->width;
    cinfo.image_height = vd->height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    z = 0;
    while(cinfo.next_scanline < vd->height) {
        int x;
        unsigned char *ptr = line_buffer;

        for(x = 0; x < vd->width; x++) {
            int r, g, b;
            int y, u, v;

            if(!z)
                y = yuyv[0] << 8;
            else
                y = yuyv[2] << 8;
            u = yuyv[1] - 128;
            v = yuyv[3] - 128;

            r = (y + (359 * v)) >> 8;
            g = (y - (88 * u) - (183 * v)) >> 8;
            b = (y + (454 * u)) >> 8;

            *(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
            *(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
            *(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);

            if(z++) {
                z = 0;
                yuyv += 4;
            }
        }

        row_pointer[0] = line_buffer;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    free(line_buffer);

    return 0;
}

int main(int argc, char *argv[])
{
    int fd;
    if(argc > 1)
        fd = open(argv[1], O_RDWR);
    else
        fd = open("/dev/video0", O_RDWR);
    if(fd < 0)
    {
        perror("open");
        return 1;
    }

    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
//check capabilities
    int ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if(ret < 0)
    {
        perror("ioctl");
        close(fd);
        return 1;
    }
    if((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0)
    {
        printf("device uable to capture\n");
        close(fd);
        return 1;
    }
    if((cap.capabilities & V4L2_CAP_STREAMING) == 0)
    {
        printf("device not suppport stream\n");
        close(fd);
        return 1;
    }
    vd.width = 640;
    vd.height = 480;
    // set paramter
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    ret = ioctl(fd, VIDIOC_S_FMT, &fmt);
    if(ret < 0)
    {
        printf("set VIDIOC_S_FMT filed\n");
        close(fd);
        return 1;
    }

    struct v4l2_requestbuffers rb;
    memset(&rb, 0, sizeof(rb));

    rb.count = 4;
    rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rb.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(fd, VIDIOC_REQBUFS, &rb);
    if(ret < 0)
    {
        printf("set VIDIOC_REQBUFS feild\n");
        close(fd);
        return 1;
    }
    if(rb.count < 2)
    {
        printf("rb.count %d\n", rb.count);
        close(fd);
        return 1;
    }
    buffers = calloc(rb.count, sizeof(struct buffer));
    int i;
    for(i = 0; i < rb.count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        if(ret < 0) {
            perror("Unable to query buffer");
            break;
        }
        buffers[i].length = buf.length;
        buffers[i].start = mmap(0 /* start anywhere */ ,
                          buf.length, PROT_READ, MAP_SHARED, fd,
                          buf.m.offset);
        if(buffers[i].start == MAP_FAILED) {
            printf("length: %u offset: %u\n", buf.length, buf.m.offset);
            perror("Unable to map buffer");
            break;
        }
    }
    /* Queue the buffers. */
    for(i = 0; i < rb.count; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        if(ret < 0) {
            perror("Unable to queue buffer");
            break;
        }
    }

    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_STREAMON, &type);
    if(ret != 0) {
        perror("Unable to start capture");
        return ret;
    }

    unsigned char *datbuf = NULL;
    unsigned long datalen = 0;
    while(1)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        ret = ioctl(fd, VIDIOC_DQBUF, &buf);
        if(ret < 0) {
            perror("Unable to dequeue buffer");
            break;
        }

        FILE *pf = fopen("video.jpeg", "w");
        if(pf == NULL)
            perror("fopen");
        else
        {
            vd.framebuffer = buffers[buf.index].start;
            compress_yuyv_to_jpeg(&vd, &datbuf, &datalen, 80);
            fwrite(datbuf, 1, datalen, pf);
            fclose(pf);
        }

        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        if(ret < 0) {
            perror("Unable to requeue buffer");
            break;
        }
        sleep(2);
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
    if(ret != 0) {
        perror("Unable to stop capture");
        return ret;
    }
    for(i = 0; i < rb.count; ++i) {
        munmap(buffers[i].start, buffers[i].length);
    }
    free(buffers);
    close(fd);
    return 0;
}
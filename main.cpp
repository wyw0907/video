#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <getopt.h>
#include <cv.h>
#include <opencv/highgui.h>
#include <jpeglib.h>
#include <opencv2/opencv.hpp>

/* the boundary is used for the M-JPEG stream, it separates the multipart stream of pictures */
#define BOUNDARY "boundarydonotcross"

typedef struct
{
    uint16_t port;
    char *host;
}config_t;

config_t conf = {0};

static bool ipl2jpeg(IplImage *img, unsigned char **outbuffer, unsigned long*outlen)
{
    unsigned char *outdata = (uchar *)img->imageData;
    struct jpeg_compress_struct cinfo = { 0 };
    struct jpeg_error_mgr jerr;
    JSAMPROW row_ptr[1];
    int row_stride;

    if(outbuffer == NULL)
        return false;
    *outbuffer = NULL;
    *outlen = 0;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    

    cinfo.image_width = img->width;
    cinfo.image_height = img->height;
    cinfo.input_components = img->nChannels;
    cinfo.in_color_space = JCS_RGB;

    jpeg_mem_dest(&cinfo, outbuffer, outlen);

    jpeg_set_defaults(&cinfo);
    jpeg_start_compress(&cinfo, TRUE);
    row_stride = img->width * img->nChannels;


    while (cinfo.next_scanline < cinfo.image_height)
    {
        row_ptr[0] = &outdata[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, row_ptr, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    return true;
}

/******************************************************************************
Description.: Display a help message
Input Value.: argv[0] is the program name and the parameter progname
Return Value: -
******************************************************************************/
void help(char *progname)
{
    fprintf(stderr, "-----------------------------------------------------------------------\n");
    fprintf(stderr, "Usage:\n" \
            " [-p | --port \"<80> [parameters]\"\n" \
            " [-h | --host \"<localhost> [parameters]\"\n" \
            " [--help ]........: display this help\n" \
            " [-v | --version ].....: display version information\n");
    fprintf(stderr, "-----------------------------------------------------------------------\n");
}

void get_option(int argc, char *argv[])
{
    while(1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = {
            {"help", no_argument, 0, 0},
            {"p", required_argument, 0, 0},
            {"port", required_argument, 0, 0},
            {"h", required_argument, 0, 0},
            {"host", required_argument, 0, 0},
            {"v", no_argument, 0, 0},
            {"version", no_argument, 0, 0},
            {0, 0, 0, 0}
        };

        c = getopt_long_only(argc, argv, "", long_options, &option_index);

        /* no more options to parse */
        if(c == -1) break;

        /* unrecognized option */
        if(c == '?') {
            help(argv[0]);
            exit(0);
        }

        switch(option_index) {
            /* help */
        case 0:
            help(argv[0]);
            exit(0);

            /* p, port */
        case 1:
        case 2:
            conf.port = atoi(optarg);
            break;

            /* h, host */
        case 3:
        case 4:
            conf.host = strdup(optarg);
            break;

            /* v, version */
        case 5:
        case 6:
            printf("myhttp Version: v1.0.0\n" \
            "Compilation Date.....: %s\n" \
            "Compilation Time.....: %s\n",
            __DATE__, __TIME__);
            exit(0);

        default:
            help(argv[0]);
            exit(0);
        }
    }

    if(conf.port == 0)
        conf.port = 8890;
    if(conf.host == NULL)
        conf.host = strdup("localhost");
}

int socket_setup(void)
{
	int sockfd;
	struct hostent *host;
	struct sockaddr_in serv_addr;

    if((host = gethostbyname(conf.host))==NULL)
	{
		perror("gethostbyname");
		exit(1);
	}

	/*创建socket*/
	if((sockfd=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		perror("socket");
		exit(1);
	}

	serv_addr.sin_family=AF_INET;
	serv_addr.sin_port=htons(conf.port);
	serv_addr.sin_addr=*((struct in_addr*)host->h_addr);
	bzero(&(serv_addr.sin_zero),8);

	/*调用connect函数主动发起对服务器的连接*/

	if(connect(sockfd,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr)) == -1)
	{
		perror("connect");
		exit(1);
	}
    return sockfd;
}

bool send_video(IplImage* pfame, int fd)
{
    char buffer[1024];
    unsigned char *data = NULL;
    unsigned long datalen;
    ipl2jpeg(pfame, &data, &datalen);
    /*
    * print the individual mimetype and the length
    * sending the content-length fixes random stream disruption observed
    * with firefox
    */
    sprintf(buffer, "Content-Type: image/jpeg\r\n" \
            "Content-Length: %08d\r\n" \
            "X-Timestamp: %06d.%06d\r\n" \
            "\r\n", (int)datalen, 0, 0);
    if(write(fd, buffer, strlen(buffer)) < 0) 
        return false;

    if(write(fd, data, datalen) < 0) 
        return false;
    sprintf(buffer, "\r\n--" BOUNDARY "\r\n");
    if(write(fd, buffer, strlen(buffer)) < 0) 
        return false;
    return true;
}

int main(int argc, char *argv[])
{
    get_option(argc, argv);
    int sockfd = socket_setup();
    
    CvCapture* pCapture = cvCreateCameraCapture(0);
    cvNamedWindow("Video", 1);
    
    while(1)
    {
        IplImage* pFrame=cvQueryFrame( pCapture );
        if(!pFrame)break;
        cvShowImage("Video",pFrame);
        send_video(pFrame, sockfd);
        char c=cvWaitKey(33);
        if(c==27)break;
    }
    cvReleaseCapture(&pCapture);
    cvDestroyWindow("Video");
    return 0;
}

#include <cv.h>
#include <opencv/highgui.h>
#include <stdio.h>
#include <jpeglib.h>
#include <opencv2/opencv.hpp>
using namespace cv;

int main(int argc, char *argv[])
{
      CvCapture* pCapture = cvCreateCameraCapture(0);
      cvNamedWindow("Video", 1);
      
      while(1)
      {
          IplImage* pFrame=cvQueryFrame( pCapture );
          if(!pFrame)break;
          cvShowImage("Video",pFrame);
          char c=cvWaitKey(33);
          if(c==27)break;
      }
      cvReleaseCapture(&pCapture);
      cvDestroyWindow("Video");
      return 0;
}

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
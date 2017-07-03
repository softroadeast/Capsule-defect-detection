#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

#include <cstdio>
#include <iostream>
#include <math.h>
#include <fstream>
#include<iomanip>
using namespace cv;
using namespace std;
int kernel_size = 3;
char print_flag ;
static void help()
{
	cout << "请在image窗口用鼠标分别对不同的区域(背景、体区、帽区、套合区)进行标注，然后按空格键，以完成分水岭算法，然后才能开始进行识别" << endl;
	cout << "识别结果在命令窗口给出"<<endl;
}
Mat markerMask, img;
Point prevPt(-1, -1);

static void onMouse(int event, int x, int y, int flags, void*)
{
	if (x < 0 || x >= img.cols || y < 0 || y >= img.rows)
		return;
	if (event == CV_EVENT_LBUTTONUP || !(flags & CV_EVENT_FLAG_LBUTTON))
		prevPt = Point(-1, -1);
	else if (event == CV_EVENT_LBUTTONDOWN)
		prevPt = Point(x, y);
	else if (event == CV_EVENT_MOUSEMOVE && (flags & CV_EVENT_FLAG_LBUTTON))
	{
		Point pt(x, y);
		if (prevPt.x < 0)
			prevPt = pt;
		line(markerMask, prevPt, pt, Scalar::all(255), 5, 8, 0);
		line(img, prevPt, pt, Scalar::all(255), 5, 8, 0);
		prevPt = pt;
		imshow("image", img);
	}
}

int main(int argc, char** argv)
{
	char* filename = argc >= 2 ? argv[1] : (char*)"D://flaw2.BMP";
	Mat img0 = imread(filename, 1), imgGray;
	ofstream outfile;
	outfile.open("myfile.txt");
	print_flag = 0;
	if (img0.empty())
	{
		cout << "Couldn'g open image " << filename << "\n";
		return 0;
	}
	help();
	namedWindow("image", 1);
	namedWindow("markerMask", 1);

//Canny
	Mat src_gray, src_canny, src_blur;
	cvtColor(img0, src_gray, CV_BGR2GRAY);
	blur(src_gray, src_blur, Size(3, 3));
	imshow("Blur", src_blur);
	Canny(src_blur, src_canny, 25, 80, kernel_size);
	imshow("Canny",src_canny);
//Hough
	Mat src_hough;
	char warp_flag = 0;
	img0.copyTo(src_hough);
	vector<Vec4i> lines;
	double angle_sum=0,angle_avr=0;
	//					last three:	threshold	minLinLength maxLineGap
	HoughLinesP(src_canny, lines, 1, CV_PI / 180, 30, 100, 10);
	if (lines.size() > 0)
	{
		vector<double> angle(lines.size());
		for (size_t i = 0; i < lines.size(); i++)
		{
			Vec4i l = lines[i];
			line(src_hough, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(0, 0, 255), 3, CV_AA);
			angle[i] = atan2((l[1] - l[3]),( l[2] - l[0]));
			printf("%d %d %d %d %f\n", l[0],l[1],l[2],l[3],angle[i]);
			angle_sum += angle[i];
		}
		angle_avr = angle_sum / lines.size();
		angle_avr = angle_avr / CV_PI * 180;
		if (angle_avr < 0)
		{
			angle_avr += 180;
		}
		printf("avrage is %f\n", angle_avr);
		if (angle_avr==90.00)
		{
			printf("no warp need\n");
			warp_flag = 0;
		}
		else
		{
			warp_flag = 1;
		}
	}
	else
	{
		printf("no warp need\n");
		warp_flag = 0;
	}
	imshow("Hough",src_hough);

//仿射变换
	Mat src_warp;
	if (warp_flag == 1)
	{
		warp_flag = 0;
		Mat rot_mat(2, 3, CV_32FC1);
		Point center = Point(img0.cols / 2, img0.rows / 2);
		double scale = 1;


		//-表示顺时针 +表示逆时针
		rot_mat = getRotationMatrix2D(center, (90 - angle_avr), scale);
		warpAffine(img0, src_warp, rot_mat, src_warp.size());
		
	}
	else
	{
		img0.copyTo(src_warp);
	}
	imshow("Warp", src_warp);
	src_warp.copyTo(img);
	imshow("image", img);
	setMouseCallback("image", onMouse, 0);
	for (;;)
	{

		cvtColor(img, markerMask, COLOR_BGR2GRAY);
		cvtColor(markerMask, imgGray, COLOR_GRAY2BGR);
		markerMask = Scalar::all(0);

		int c = waitKey(0);

		if ((char)c == 27)
			break;
		if ((char)c == 'w' || (char)c == ' ')
		{
//分水岭
			int i, j, compCount = 0;
			vector<vector<Point> > contours;
			vector<Vec4i> hierarchy;

			imshow("markerMask", markerMask);
			findContours(markerMask, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE);

			if (contours.empty())
				continue;
			Mat markers(markerMask.size(), CV_32S);
			markers = Scalar::all(0);
			int idx = 0;
			for (; idx >= 0; idx = hierarchy[idx][0], compCount++)
				drawContours(markers, contours, idx, Scalar::all(compCount + 1), -1, 8, hierarchy, INT_MAX);
//				drawContours(markers, contours, idx, 255, -1, 8, hierarchy, INT_MAX);

			//利用下面一段信息进行长度检测
			int count = 0;
			for (i = 0; i < markers.rows; i++)
			{
				for (j = 0; j < markers.cols; j++)
				{
					int number = markers.at<int>(i, j);
					if (number != 0)
					{
						count++;
		//				printf("%d (%d %d) ", markers.at<int>(i, j),i,j);
					}
				}
			}
			printf("%d \n", count);
			if (compCount == 0)
				continue;

			vector<Vec3b> colorTab;
			for (i = 0; i < compCount; i++)
			{
				int b = theRNG().uniform(0, 255);
				int g = theRNG().uniform(0, 255);
				int r = theRNG().uniform(0, 255);

				colorTab.push_back(Vec3b((uchar)b, (uchar)g, (uchar)r));
			}

			double t = (double)getTickCount();
			Mat src_wshed;
			src_warp.copyTo(src_wshed);
			watershed(src_wshed, markers);
			t = (double)getTickCount() - t;
			printf("execution time = %gms\n", t*1000. / getTickFrequency());

			Mat wshed(markers.size(), CV_8UC3);
			Mat src_long(markers.size(), CV_8UC1);
			src_long = Scalar::all(0);

			// paint the watershed image
//			for (i = 0; i < markers.rows; i++)
//			for (j = 0; j < markers.cols; j++)
			for (i = 1; i < markers.rows-1; i++)
			for (j = 1; j < markers.cols-1; j++)
			{
				int index = markers.at<int>(i, j);
				//-1则表示为分水岭，否则表示汇聚盆
				if (index == -1)
				{
					wshed.at<Vec3b>(i, j) = Vec3b(255, 255, 255);
					//分水岭用白色表示
					src_long.at<char>(i, j) = 255;   
				}
				else if (index <= 0 || index > compCount)
					wshed.at<Vec3b>(i, j) = Vec3b(0, 0, 0);
				else
				{
					wshed.at<Vec3b>(i, j) = colorTab[index - 1];
					//不同区域不同像素
					src_long.at<char>(i, j) = index;
				}
					
			}

			wshed = wshed*0.5 + imgGray*0.5;
			imshow("watershed transform", wshed);

//长度检测
			Mat long_show;
			src_long.copyTo(long_show);
			int comp_temp;
			int count3=0, count2=0, count1=0,count0=0;
			int row3=0, row2=0, row1=0,row0=0;
			int flag3=0, flag2=0, flag1=0,flag0=0;
			Point start, end;
			for (i = 0; i < src_long.rows; i++)
			{
				for (j = 0; j < src_long.cols; j++)
				{
					comp_temp = src_long.at<char>(i, j);
//					printf("%d (%d %d) ",comp_temp,i,j);
//					outfile << comp_temp <<"( "<<i<<","<<j<<" )"<< endl;
					switch (comp_temp)
					{
					case 3:
						count3++; break;
					case 2:
						count2++; break;
					case 1:
						count1++; break;
					default:
						break;
					}
					if (comp_temp == 1 && flag1 == 1)
					{
						count0++;
						
					}
				}
				if (flag0 == 0 && flag1 == 1 && count0 == 0) { row0 = i; flag0 = 1; }
				if (flag3 == 0 && count3 >= 2) { row3 = i; flag3 = 1; }
				if (flag2 == 0 && count2 >= 10) {  row2 = i; flag2 = 1; }
				if (flag1 == 0 && count1 >20) {  row1 = i; flag1 = 1; }

//				printf("count1 = %d count0 = %d row = %d flag1= %d flag0=%d \n", count1,count0,i,flag1,flag0);
				count3 = 0; count2 = 0; count1 = 0; count0 = 0;
			}
			printf("\n row3 is %d,row2 is %d,row1 is %d,row0 is %d\n",row3,row2,row1,row0);
			start.x = 0; start.y = row3; end.x = src_long.cols - 1; end.y = row3;
			line(long_show, start, end, Scalar(255), 3, 8);
			start.x = 0; start.y = row2; end.x = src_long.cols - 1; end.y = row2;
			line(long_show, start, end, Scalar(255), 3, 8);
			start.x = 0; start.y = row1; end.x = src_long.cols - 1; end.y = row1;
			line(long_show, start, end, Scalar(255), 3, 8);
			start.x = 0; start.y = row0; end.x = src_long.cols - 1; end.y = row0;
			line(long_show, start, end, Scalar(255), 3, 8);
			imshow("long", long_show);


			int long_hat, long_body, long_both;
			float body_hat, both_hat;
			long_hat = row0 - row2;
			long_body = row0 - row3;
			long_both = row1 - row2;
			body_hat = (float)long_body /(float) long_hat;
			both_hat = (float)long_both / (float)long_hat;
			printf("body = %d hat = %d both = %d body_hat=%f both_hat=%f\n", long_body, long_hat, long_both, body_hat, both_hat);
			if (body_hat < 1.8&&both_hat>0.7)
			{
				printf("过短\n");
				continue;
			}

//生成掩膜
			//生成矩形框
			Mat src_mask;
			vector<vector<Point> > contours2;
			vector<Vec4i> hierarchy2;
			src_long.copyTo(src_mask);
			threshold(src_mask, src_mask, 240, 255, THRESH_BINARY);
			//for (i = 0; i < src_mask.rows; i++)
			//{
			//	for (j = 0; j < src_mask.cols; j++)
			//	{
			//		comp_temp = src_mask.at<uchar>(i, j);
			//		outfile << comp_temp << ",";
			//	}
			//	outfile << endl;
			//}
			//src_warp.copyTo(src_mask);
			//cvtColor(src_mask, src_mask, CV_BGR2GRAY);
			//blur(src_mask, src_mask, Size(3, 3));
			//threshold(src_mask, src_mask, 40, 255, THRESH_BINARY);
			//int erosion_type = MORPH_RECT; int erosion_size = 2;
			//Mat element = getStructuringElement(erosion_type,
			//	Size(2 * erosion_size + 1, 2 * erosion_size + 1),
			//	Point(erosion_size, erosion_size));
			//erode(src_mask, src_mask, element);
			imshow("src_mask", src_mask);

			findContours(src_mask, contours2, hierarchy2, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));
			vector<Point> contours_poly;
			Rect boundRect;
			printf("the number is %d\n", contours2.size());
			vector<double> area(contours2.size());
			double maxArea = 0;
			int maxId = 0;
			for (int i = 0; (i < contours2.size()); i++)
			{

				area[i] = contourArea(contours2[i]);
				if (area[i] >= maxArea)
				{
					maxArea = area[i];
					maxId = i;
				}
		//		printf("maxId is %d\n", maxId);

			}
			approxPolyDP(Mat(contours2[maxId]), contours_poly, 3, false);
			boundRect = boundingRect(Mat(contours_poly));
			printf("area is %d\n", boundRect.area());

			Mat drawing = Mat::zeros(src_warp.size(), CV_8UC1);
			drawContours(drawing, contours2, maxId, 255, 1, 8, vector<Vec4i>(), 0, Point());
			Mat src_contours;
			drawing.copyTo(src_contours);
			////top left buttom right
			rectangle(drawing, boundRect.tl(), boundRect.br(), 255, 2, 8, 0);
			printf("rect's width is %d\n", boundRect.width);
			imshow("Rect", drawing);

//切边检测
			int count_temp = 0;
			for (i = 1; i < src_long.rows - 1; i++)
			{
				for (j = 1; j < src_long.cols - 1; j++)
				{
					if (-1 == src_long.at<char>(i, j)) count_temp++;

					//					outfile << count_temp << setw(3) << "( " << i << setw(3) << "," << j << setw(3) << " )";
				}
				//				printf("%d,%d\n",i,count_temp);
//				outfile << i << "," << count_temp << endl;
				if (count_temp!=0)
				break;
				//count_temp = 0;
				//				outfile  << endl;
			}
			float count_width;
			count_width = float(count_temp) / float(boundRect.width);
			printf("count_temp %d\n", count_temp);
			printf("count_temp/boundRect.width is %f\n", count_width);
			if (count_width > 0.4)
			{
				printf("切边\n");
				continue;
			}
			
			//ROI
			Mat warp_roi(src_warp, Rect(boundRect.tl().x, boundRect.tl().y, boundRect.width, boundRect.height));
			imshow("WARP_ROI", warp_roi);

//双帽检测
			printf("height is %d,width is %d\n", warp_roi.rows, warp_roi.cols);
			float sum_top=0, sum_buttom=0,sum_col=0;
			int sum_temp=0;
			for (i = 0; i < 100; i++)
			{
				for (j = 0; j < warp_roi.cols ; j++)
				{
					sum_temp = warp_roi.at<uchar>(i, j);
				//	outfile << sum_temp << "," ;
					sum_col += sum_temp;
				}
				sum_col = sum_col / (warp_roi.cols);
				sum_top += sum_col;
				sum_col = 0;
			//	outfile <<  endl;
			}
			sum_top = sum_top / 100;
			//outfile << endl;
			for (i = warp_roi.rows-100; i < warp_roi.rows; i++)
			{
				for (j = 0; j < warp_roi.cols; j++)
				{
					sum_temp = warp_roi.at<uchar>(i, j);
			//		outfile << sum_temp << ",";
					sum_col += sum_temp;
				}
				sum_col = sum_col / (warp_roi.cols);
				sum_buttom += sum_col;
				sum_col = 0;
			//	outfile << endl;
			}
			sum_buttom = sum_buttom / 100;
			float diff = sum_top - sum_buttom;
			printf("top is %f,buttom is %f,diff is %f\n", sum_top, sum_buttom, diff);
			
			if (diff <= 50)
			{
				printf("双帽\n");
				continue;
			}
//计算瘪壳 帽或者体被挤压，导致宽度不一样 前后40行是圆弧，不算，计算后面的40行
			int width_temp;
			int width_left, width_right, width_diff,width_diff_sum=0;
			int width_top=0, width_buttom=0, width_diff_avr=0;
			Mat width_roi(src_contours, Rect(boundRect.tl().x, boundRect.tl().y, boundRect.width+2, boundRect.height));
			imshow("WIDTH_ROI", width_roi);
			for (i = 40; i < 80; i++)
			{
				for (j = width_roi.cols / 2; j != 0; j--)
				{
					width_temp = width_roi.at<uchar>(i, j);
					if (width_temp == 255)
					{
						 break;
					}
				}
				width_left = j;
				for (j = width_roi.cols / 2; j != width_roi.cols-1; j++)
				{
					width_temp = width_roi.at<uchar>(i, j);
					if (width_temp == 255)
					{
						break;
					}
				}
				width_right = j;
				width_diff = width_right - width_left;
//				outfile << width_diff << endl;
				width_diff_sum += width_diff;
			}
			width_top = width_diff_sum/40;
			width_diff_sum = 0;
			
			for (i = width_roi.rows - 40; i > width_roi.rows - 80; i--)
			{
				for (j = width_roi.cols / 2; j != 0; j--)
				{
					width_temp = width_roi.at<uchar>(i, j);
					if (width_temp == 255)
					{
						break;
					}
				}
				width_left = j;
				for (j = width_roi.cols / 2; j != width_roi.cols - 1; j++)
				{
					width_temp = width_roi.at<uchar>(i, j);
					if (width_temp == 255)
					{
						break;
					}
				}
				width_right = j;
				width_diff = width_right - width_left;
//				outfile << width_diff << endl;
				width_diff_sum += width_diff;
			}
			width_buttom = width_diff_sum / 40;
			width_diff_sum = 0;
			width_diff_avr = width_top - width_buttom;
			printf("width_top is %d,width_buttom is %d,width_diff_avr is %d\n", width_top, width_buttom, width_diff_avr);
			if (abs(width_diff_avr) > 12)
			{
				printf("瘪壳\n");
				continue;
			}

//缺陷检测
			//Mat mask_roi(src_warp, Rect(boundRect.tl().x-5, boundRect.tl().y-5, boundRect.width+10, boundRect.height+10));
			//imshow("MASK_ROI", mask_roi);
			//Mat src_defect;
			//mask_roi.copyTo(src_defect);
			//threshold(src_defect, src_defect, 55, 255, THRESH_BINARY);
			//int erosion_type = MORPH_RECT; int erosion_size = 3;
			//Mat element = getStructuringElement(erosion_type,
			//	Size(2 * erosion_size + 1, 2 * erosion_size + 1));
			//erode(src_defect, src_defect, element);
			//start.x = 0; start.y = row2-row3+5; end.x = src_long.cols - 1; end.y = row2-row3+5;
			//line(src_defect, start, end, 0, 11, 8);
			//imwrite("mask.jpg", src_defect);
			//imshow("Defect", src_defect);

			Mat mask_roi(src_warp, Rect(boundRect.tl().x , boundRect.tl().y , boundRect.width , boundRect.height ));
			imshow("MASK_ROI", mask_roi);
			Mat src_defect;
			mask_roi.copyTo(src_defect);
			cvtColor(src_defect, src_defect, CV_RGB2GRAY);
			adaptiveThreshold(src_defect, src_defect, 255, CV_ADAPTIVE_THRESH_GAUSSIAN_C, CV_THRESH_BINARY, 11, 4);
	//		imshow("Defect", src_defect);
			Mat mask_made = imread("mask.jpg", 1);
			cvtColor(mask_made, mask_made, CV_RGB2GRAY);
			imshow("Mask", mask_made);

			Point2f srcTri[4];
			Point2f dstTri[4];
			srcTri[0] = Point2f(0, 0);
			srcTri[1] = Point2f(mask_made.cols-1, 0);
			srcTri[2] = Point2f(mask_made.cols - 1, mask_made.rows - 1);
			srcTri[3] = Point2f(0, mask_made.rows - 1);

			dstTri[0] = Point2f(0, 0);
			dstTri[1] = Point2f(src_defect.cols - 1, 0);
			dstTri[2] = Point2f(src_defect.cols - 1, src_defect.rows - 1);
			dstTri[3] = Point2f(0, src_defect.rows - 1);

			Mat warp_mat(3, 3, CV_32FC1);
			Mat warp_dst, warp_rotate_dst;
			warp_dst = Mat::zeros(src_defect.rows, src_defect.cols, src_defect.type());
			warp_mat = getPerspectiveTransform(srcTri, dstTri);
			warpPerspective(mask_made, warp_dst, warp_mat, warp_dst.size());
			imshow("warp_dst", warp_dst);
			bitwise_and(warp_dst, src_defect, src_defect);
			bitwise_xor(warp_dst, src_defect, src_defect);

			imshow("Defect", src_defect);
		
			vector<vector<Point> > contours3;
			vector<Vec4i> hierarchy3;
			findContours(src_defect, contours3, hierarchy3, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE, Point(0, 0));
			Mat drawing3 = Mat::zeros(src_defect.size(), CV_8UC1);
			vector<vector<Point> > contours_poly3(contours3.size());
			vector<Rect> boundRect3(contours3.size());
			for (int i = 0; i < contours3.size(); i++)
			{
				approxPolyDP(Mat(contours3[i]), contours_poly3[i], 3, true);
				boundRect3[i] = boundingRect(Mat(contours_poly3[i]));
			}
			vector<double> area3(contours3.size());
			double maxArea3=0;
			int maxId3;
			for (int i = 0; (i < contours3.size()); i++)
			{

				area3[i] = contourArea(contours3[i]);
				if (area3[i] >= maxArea3)
				{
					maxArea3 = area3[i];
					maxId3 = i;
				}
				printf("maxId3 is %d\n", maxId3);

			}
			drawContours(drawing3, contours3, maxId3, 255, 1, 8, hierarchy3, 0, Point());
			rectangle(drawing3, boundRect3[maxId3].tl(), boundRect3[maxId3].br(), 255, 2, 8, 0);
			float area_rect = boundRect3[maxId3].area();
			int height_rect = boundRect3[maxId3].height;
			int width_rect = boundRect3[maxId3].width;
			float height_width;
			height_width = (float)height_rect / (float)width_rect;
			printf("area is %f,height is %d,width is %d\n", area_rect, height_rect, width_rect);
			if (height_width < 3)
			{
				printf("破洞\n");
			}
			else
			{
				printf("裂缝\n");
			}


			imshow("Drawing3", drawing3);
			outfile.close();
		}
	}

	return 0;
}


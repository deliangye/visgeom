#include <iostream>
#include <iomanip>
#include <fstream>
#include <ctime>

#include <Eigen/Eigen>
#include <opencv2/opencv.hpp>

#include "reconstruction/curve_rasterizer.h"
#include "reconstruction/eucm_stereo.h"

using namespace cv;
using namespace std;

typedef cv::Mat_<uint8_t> Mat8;

void showHistogram(const string & name, const Mat & img)
{
    int histSize = 256;    // bin size
    float range[] = { 0, 1 };
    const float *ranges[] = { range };

    // Calculate histogram
    MatND hist;
    calcHist( &img, 1, 0, Mat(), hist, 1, &histSize, ranges, true, false );
    
    // Show the calculated histogram in command window
    double total;
    total = img.rows * img.cols;
    for( int h = 0; h < histSize; h++ )
         {
            float binVal = hist.at<float>(h);
            cout<<" "<<binVal;
         }

    // Plot the histogram
    int hist_w = 512; int hist_h = 400;
    int bin_w = cvRound( (double) hist_w/histSize );

    Mat histImage( hist_h, hist_w, CV_8UC1, Scalar( 0,0,0) );
    normalize(hist, hist, 0, histImage.rows, NORM_MINMAX, -1, Mat() );
    
    for( int i = 1; i < histSize; i++ )
    {
      line( histImage, Point( bin_w*(i-1), hist_h - cvRound(hist.at<float>(i-1)) ) ,
                       Point( bin_w*(i), hist_h - cvRound(hist.at<float>(i)) ),
                       Scalar( 255, 0, 0), 2, 8, 0  );
    }

    imshow(name, histImage );
}

int main(int argc, char** argv)
{	
    /*Polynomial2 poly2;
    poly2.kuu = -1; 
    poly2.kuv = 1; 
    poly2.kvv= -1; 
    poly2.ku = 0.25; 
    poly2.kv = 0.25; 
    poly2.k1 = 5;
    
    CurveRasterizer<Polynomial2> raster(1, 1, -100, 100, poly2);
    CurveRasterizer2<Polynomial2> raster2(1, 1, -100, 100, poly2);

    auto tr0 = clock();
    int x1 = 0;
    int x2 = 0;
    for (int i = 0; i < 10000000; i++)
    {
        raster.step();
        x1 += raster.x;
    }
    auto tr1 = clock();
    
    for (int i = 0; i < 10000000; i++)
    {
        raster2.step();
        x2 += raster2.x;
    }
    auto tr2 = clock();
    
    cout << "optimized " << double(tr1 - tr0) / CLOCKS_PER_SEC << endl;
    cout << "simple " << double(tr2 - tr1) / CLOCKS_PER_SEC << endl;
    cout << x1 << " " << x2 << endl;
    return 0;*/
    ifstream paramFile(argv[1]);
    if (not paramFile.is_open())
    {
        cout << argv[1] << " : ERROR, file is not found" << endl;
        return 0;
    }
    
    array<double, 6> params;
    
    cout << "EU Camera model parameters :" << endl;
    for (auto & p: params) 
    {
        paramFile >> p;
        cout << setw(15) << p;
    }
    cout << endl;
    paramFile.ignore();
    
    array<double, 6> cameraPose;
    cout << "Camera pose wrt the robot :" << endl;
    for (auto & e: cameraPose) 
    {
        paramFile >> e;
        cout << setw(15) << e;
    }
    cout << endl;
    paramFile.ignore();
    Transformation<double> TbaseCamera(cameraPose.data());
    
    array<double, 6> planePose;
    cout << "Plane pose :" << endl;
    for (auto & e: cameraPose) 
    {
        paramFile >> e;
        cout << setw(15) << e;
    }
    cout << endl;
    paramFile.ignore();
    Transformation<double> TbasePlane(cameraPose.data());
    
    StereoParameters stereoParams;
    paramFile >> stereoParams.u0;
    paramFile >> stereoParams.v0;
    paramFile >> stereoParams.disparityMax;
    paramFile >> stereoParams.blockSize;
    paramFile.ignore();
    
    string imageDir;
    getline(paramFile, imageDir);
    
    string imageInfo, imageName;
    array<double, 6> robotPose1, robotPose2;
    getline(paramFile, imageInfo);
    istringstream imageStream(imageInfo);
    imageStream >> imageName;
    for (auto & x : robotPose1) imageStream >> x;

    Mat8 img1 = imread(imageDir + imageName, 0);
    int counter = 2;
    while (getline(paramFile, imageInfo))
    {
        istringstream imageStream(imageInfo);
        
        imageStream >> imageName;
        for (auto & x : robotPose2) imageStream >> x;
    
        Transformation<double> T01(robotPose1.data()), T02(robotPose2.data());
        Transformation<double> TleftRight = T01.compose(TbaseCamera).inverseCompose(T02.compose(TbaseCamera));
        
        
        Mat8 img2 = imread(imageDir + imageName, 0);

        EnhancedStereo stereo(TleftRight, img1.cols, img1.rows, params.data(), params.data(), stereoParams);

        cv::Mat_<uint8_t> res;
        auto t2 = clock();
        stereo.comuteStereo(img1, img2, res);
        auto t3 = clock();
//        cout << double(t3 - t2) / CLOCKS_PER_SEC << endl;
        Mat_<float> distMat;
        Mat_<float> planeMat;
        
        stereo.computeDistance(distMat);
        Transformation<double> T0Camera = T01.compose(TbaseCamera);
        stereo.generatePlane(T0Camera.inverseCompose(TbasePlane), planeMat,
         vector<Vector3d>{Vector3d(-0.1, -0.1, 0), Vector3d(-0.1 + 3 * 0.45, -0.1, 0),
                          Vector3d(-0.1 + 3 * 0.45, 0.5, 0), Vector3d(-0.1, 0.5, 0) } );
        imshow("dist" + to_string(counter) , distMat);
        imshow("plane" , planeMat);
        imwrite("/home/bogdan/projects/plane.png", planeMat);
        double err = 0;
        double err2 = 0;
        double dist = 0;
        int N = 0;
        int Nmax = 0;
        Mat_<float> inlierMat(planeMat.size());
        inlierMat.setTo(0);
        for (int u = 0; u < distMat.cols; u++)
        {
            for (int v = 0; v < distMat.rows; v++)
            {
                
                if (planeMat(v, u) == 0) continue;
                Nmax++;
                dist += planeMat(v, u);
                inlierMat(v, u) = 1;
                if (distMat(v, u) == 0 or distMat(v, u) != distMat(v, u) or planeMat(v, u) != planeMat(v, u)) continue;
                if (abs(distMat(v, u) - planeMat(v, u)) > 0.10) continue;
                inlierMat(v, u) = 0;
                err += distMat(v, u) - planeMat(v, u);
                err2 += pow(distMat(v, u) - planeMat(v, u), 2);
                N++;
            }
        }
//        cout << (counter - 1) * 7 << " & " << dist/ Nmax * 1000 << " & " << err / N *1000 << " & " << sqrt(err2 / N)*1000  
//                << " & " << 100 * N / double(Nmax) << "\\\\" << endl << "\\hline" << endl;
        cout << "avg err : " << err / N *1000 << " avg err2 : " 
<< sqrt(err2 / N)*1000  << " number of inliers : " << 100 * N / double(Nmax) << endl;
        imshow("diff" + to_string(counter), abs(planeMat - distMat));
        imshow("inliers" + to_string(counter), inlierMat);
        counter++;
        
    }
    waitKey();
    return 0;
}




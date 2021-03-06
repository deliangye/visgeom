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
        cout << setw(10) << p;
    }
    cout << endl;
    paramFile.ignore();
    
    array<double, 6> cameraPose;
    cout << "Camera pose wrt the robot :" << endl;
    for (auto & e: cameraPose) 
    {
        paramFile >> e;
        cout << setw(10) << e;
    }
    cout << endl;
    paramFile.ignore();
    Transformation<double> TbaseCamera(cameraPose.data());
    
    array<double, 6> robotPose1, robotPose2;

    
    
    StereoParameters stereoParams;
    paramFile >> stereoParams.u0;
    paramFile >> stereoParams.v0;
    paramFile >> stereoParams.disparityMax;
    paramFile >> stereoParams.blockSize;
    paramFile.ignore();
    string imageDir;
    getline(paramFile, imageDir);
    string imageInfo, imageName;
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
        cout << double(t3 - t2) / CLOCKS_PER_SEC << endl;
        imwrite(imageDir + "res" + to_string(counter++) + ".png", res*3);
    }
    return 0;
}




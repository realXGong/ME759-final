#include <iostream>
#include <vector>
#include <string>
#include "opencv2/core.hpp"

#ifdef HAVE_OPENCV_XFEATURES2D
#include "opencv2/calib3d.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/xfeatures2d.hpp"

#include <chrono>
#include <ratio>
#include <cmath>

using std::chrono::high_resolution_clock;
using std::chrono::duration;
using namespace std;
using namespace cv;
using namespace cv::xfeatures2d;

static void help()
{
    cout << "\nThis program demonstrates using features detector, descriptor extractor and Matcher" << endl;
    cout << "\nUsage:\n\tpanaroma_stitcher <number of images> <image1> <image2> ... <imageN>" << endl;
}


int main(int argc, char* argv[])
{
    cout << "\n Start Stitching: Sequential Optimatized mode" << endl;

    if (argc < 3)
    {
        help();
        return -1;
    }

    int n = 0;
    try{
        n = stoi(argv[1]);
        cout << "Input: " << n << " images" << endl;

        if(n < 2 || argc < 2 + n){
            help();
            return -1;
        }
    }
    catch (std::invalid_argument const &e){
        help();
	}

    high_resolution_clock::time_point start;
    high_resolution_clock::time_point end;
    duration<double, std::milli> duration_sec;


    Mat* imgs = new Mat[n];

    // TODO check input image size matches
    for (int i = 0; i < n; ++i)
    {
        imgs[i] = imread(argv[i+2], IMREAD_GRAYSCALE);
        CV_Assert(!imgs[i].empty());
    }

    start = high_resolution_clock::now();
    // detecting keypoints & computing descriptors for all imgs
    double minHessian = 400;
    Ptr<SURF> detector = SURF::create(minHessian);
    vector<KeyPoint>* keypoints = new vector<KeyPoint>[n];
    Mat* descriptors = new Mat[n];
    for(int i = 0; i < n; i++){
        detector->detectAndCompute(imgs[i], noArray(), keypoints[i], descriptors[i]);
    }
    for(int i = 0; i < n; i++){
        cout << "FOUND " << keypoints[i].size() << " keypoints on image " << i << endl;
    }

    // Match descriptors with FLANN based matcher
    // Match img1 & img2; img2 & img3; ... ; img(n-1) & img(n)
    Ptr<DescriptorMatcher> matcher = DescriptorMatcher::create(DescriptorMatcher::FLANNBASED);
    vector<vector<DMatch>>* knn_matches = new vector<vector<DMatch>>[n-1];
    for(int i = 0; i < n-1; i++){
        matcher->knnMatch(descriptors[i], descriptors[i+1], knn_matches[i], 2);
    }

    // Filter matches using the Lowe's ratio test
    // For n images, number of matches is n - 1
    // Can use OpenMP
    const float ratio_thresh = 0.75f;
    vector<DMatch>* good_matches = new vector<DMatch>[n-1];
    for(int i = 0; i < n-1; i++){
        for (size_t j = 0; j < knn_matches[i].size(); j++){
            if (knn_matches[i][j][0].distance < ratio_thresh * knn_matches[i][j][1].distance){
                good_matches[i].push_back(knn_matches[i][j][0]);
            }
        }
    }

    // drawing the results
    Mat* img_matches = new Mat[n-1];
    for(int i = 0; i < n-1; i++){
        drawMatches(imgs[i], keypoints[i], imgs[i+1], keypoints[i+1], good_matches[i], img_matches[i], Scalar::all(-1),
                 Scalar::all(-1), std::vector<char>(), DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS );
    }

    // print out matches
    /*
    for(int i = 0; i < n-1; i++){
        namedWindow(to_string(i), WINDOW_AUTOSIZE);
        imshow(to_string(i), img_matches[i]);
        waitKey(0);
    }
    */
    //imwrite("matches.jpg", img_matches);
    
    // Localize the object and get keypoints from the good matches
    // Each non-head nor non-tail img has two set of matching keypoints. 
    // E.g. img1 has matching keypoints with img0, and matching keypoints with img2
    // Let each img take 2 spot in array objs. E.g. img0 taks objs[0], objs[1]
    vector<Point2f>* objs = new vector<Point2f>[2*n-2];
    for(int i = 0; i < n-1; i++){
        for(int j = 0; j < good_matches[i].size(); j++){
            //-- Get the keypoints from the good matches
            objs[2*i].push_back(keypoints[i][ good_matches[i][j].queryIdx ].pt );
            objs[2*i+1].push_back(keypoints[i+1][ good_matches[i][j].trainIdx ].pt );
        }
    }

    // Find homography matrix
    // Note: order of obj2, obj1 does matter
    // Use obj1/img (the leftmost image) as reference perspective
    Mat* Hs = new Mat[n-1];
    for(int i = 0; i < n-1; i++){
        Hs[i] = cv::findHomography(objs[2*i+1], objs[2*i], RANSAC);
        cout << "Find homography pair " << i << " and " << i+1 << endl;
    }

    Mat img_pano = imgs[n-1];
    // Start from right-most imgs. Shifting perpespetive while proceeding
    for(int i = n-1; i > 0; i--){
        warpPerspective(img_pano, img_pano, Hs[i-1], Size(img_pano.cols + imgs[i-1].cols, img_pano.rows));
        Mat left_half = img_pano(Rect(0, 0, imgs[i-1].cols, imgs[i-1].rows));  
        imgs[i-1].copyTo(left_half);
        cout << "finish pair " << i << " and " << i+1 << endl;
    }
    end = high_resolution_clock::now();
    duration_sec = std::chrono::duration_cast<duration<double, std::milli>>(end - start);
    cout << "Panaroma stitching completed. Time taken: " << duration_sec.count() << endl;

    imshow("Pano", img_pano);
    waitKey(0);
    return 0;
}


#else

int main()
{
    std::cerr << "Error: Panaroma stitching requires OpenCV xfeatures2d module" << std::endl;
    return 0;
}

#endif

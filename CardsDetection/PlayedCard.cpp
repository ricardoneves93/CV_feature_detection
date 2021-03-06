#include "PlayedCard.h"

using namespace std;
using namespace cv;

int computeSurfGoodMatches(Mat, Mat);

PlayedCard::PlayedCard () {
	this->originalImg = NULL;
	this->rotatedImg = NULL;
	this->leastDifferentCard = NULL;
	this->differences.clear();
}
// mode = 0 -> Use subtraction / mode = 1 -> Use surf
PlayedCard::PlayedCard(Mat originalImg, Mat rotatedImg, vector<Point> contours, vector<Point2f> cornerPoints, vector<Card*> deck, int mode, int cardIndex) {
	this->originalImg = originalImg;
	this->rotatedImg = rotatedImg;
	this->leastDifferentCard = NULL;
	this->cornerPoints = cornerPoints;
	this->contours = contours;
	this->cardIndex = cardIndex;

	
	// Compute the difference with all deck cards
	if (mode == 0)
		computeAbsDifference(deck);
	else if (mode == 1) {
		int progress = 52;
		progress += 12 * cardIndex;

		if (progress % 2 == 0) {
			computeKeypoints(); progress += 1;
			showLoadingBar(progress);
			computeDescriptors(); progress += 1;
			showLoadingBar(progress);
			computeDifferenceSurf(deck, progress, mode);
		}
		else {
			computeKeypoints();
			computeDescriptors();
			computeDifferenceSurf(deck, 0, mode);
		}
	}
}

void PlayedCard::showLoadingBar(int progress) {
	if (progress >= 99)
		progress = 100;
	cout << "\r";
	cout << "[";
	for (int j = 0; j < progress / 2; j++) {
		cout << "=";
	}
	cout << ">] " << progress << "%";
}

Mat PlayedCard::getOriginalImg() {
	return originalImg;
}

Mat PlayedCard::getRotatedImg() {
	return rotatedImg;
}

void PlayedCard::setRotatedImg(cv::Mat img) {
	this->rotatedImg = img;
}

map<Card*, int> PlayedCard::getCardDifferences() {
	return differences;
}

bool pairCompare(pair<Card*, int> p, pair<Card*, int> p1) {
	return p.second < p1.second;
}

std::vector<cv::Point2f> PlayedCard::getCornerPoints() {
	return this->cornerPoints;
}

vector<KeyPoint> PlayedCard::getKeypointsOriginal() {
	return keypointsOriginal;
}

cv::Mat PlayedCard::getDescriptorsOriginal() {
	return descriptorsOriginal;
}

vector<KeyPoint> PlayedCard::getKeypointsRotated() {
	return keypointsRotated;
}

cv::Mat PlayedCard::getDescriptorsRotated() {
	return descriptorsRotated;
}

void PlayedCard::computeKeypoints() {
	int minHessian = 1000;
	SurfFeatureDetector detector(minHessian);
	detector.detect(originalImg, keypointsOriginal);
	detector.detect(rotatedImg, keypointsRotated);
}

void PlayedCard::computeDescriptors() {
	SurfDescriptorExtractor extractor;
	extractor.compute(originalImg, keypointsOriginal, descriptorsOriginal);
	extractor.compute(rotatedImg, keypointsRotated, descriptorsRotated);
}

void PlayedCard::computeDifferenceSurf(vector<Card*> deck, int progress, int mode) {

	float progress_aux = (float) progress;
	for (int i = 0; i < deck.size(); i++) {
		int goodMatchesOriginal = computeSurfGoodMatches(keypointsOriginal, deck[i]->getKeypoints(), descriptorsOriginal, deck[i]->getDescriptors());
		int goodMatchesRotated = computeSurfGoodMatches(keypointsRotated, deck[i]->getKeypoints(), descriptorsRotated, deck[i]->getDescriptors());

		int matches = goodMatchesOriginal + goodMatchesRotated;
		differences.insert(pair<Card*, int>(deck[i], matches));
		progress_aux += (float) 0.185185;

		if (mode == 1 && i % 6 == 0) {
			showLoadingBar( (int) progress_aux);
		}
	}

	pair<Card*, int> max = *max_element(differences.begin(), differences.end(), pairCompare);
	this->leastDifferentCard = max.first;
}

int PlayedCard::computeSurfGoodMatches(vector<KeyPoint> keypoints_1, vector<KeyPoint> keypoints_2, Mat descriptors_1, Mat descriptors_2) {

	//-- Step 3: Matching descriptor vectors using FLANN matcher
	FlannBasedMatcher matcher;
	vector< DMatch > matches;
	matcher.match(descriptors_1, descriptors_2, matches);

	filterMatchesByAbsoluteValue(matches, 0.125);
	filterMatchesRANSAC(matches, keypoints_1, keypoints_2);

	return (int)matches.size();
}

void PlayedCard::filterMatchesByAbsoluteValue(vector<DMatch> &matches, float maxDistance) {
	vector<DMatch> filteredMatches;
	for (size_t i = 0; i<matches.size(); i++)
	{
		if (matches[i].distance < maxDistance)
			filteredMatches.push_back(matches[i]);
	}
	matches = filteredMatches;
}

Mat PlayedCard::filterMatchesRANSAC(vector<DMatch> &matches, vector<KeyPoint> &keypointsA, vector<KeyPoint> &keypointsB) {
	Mat homography;
	std::vector<DMatch> filteredMatches;
	if (matches.size() >= 4)
	{
		vector<Point2f> srcPoints;
		vector<Point2f> dstPoints;
		for (size_t i = 0; i<matches.size(); i++)
		{

			srcPoints.push_back(keypointsA[matches[i].queryIdx].pt);
			dstPoints.push_back(keypointsB[matches[i].trainIdx].pt);
		}

		Mat mask;
		homography = findHomography(srcPoints, dstPoints, CV_RANSAC, 3.0, mask);

		for (int i = 0; i<mask.rows; i++)
		{
			if (mask.ptr<uchar>(i)[0] == 1)
				filteredMatches.push_back(matches[i]);
		}
	}
	matches = filteredMatches;
	return homography;
}

void PlayedCard::computeAbsDifference(vector<Card*> deck) {
	
	for (int i = 0; i < deck.size(); i++) {
		Mat diff;
		Mat diffRotated;

		absdiff(this->originalImg, deck[i]->getCardImg(), diff);
		absdiff(this->rotatedImg, deck[i]->getCardImg(), diffRotated);

		GaussianBlur(diff, diff, Size(5, 5), 5);
		threshold(diff, diff, 200, 255, CV_THRESH_BINARY);

		GaussianBlur(diffRotated, diffRotated, Size(5, 5), 5);
		threshold(diffRotated, diffRotated, 200, 255, CV_THRESH_BINARY);

		Scalar sOriginal(sum(diff));
		Scalar sRotated(sum(diffRotated));

		// Mean value between rotated and non rotated cards
		int diffValue = ((int) sOriginal[0] + (int) sRotated[0] ) / 2;

		// Insert the corresponding value in the card
		differences.insert(pair<Card*, int>(deck[i], diffValue));
	}

	// Get the card with the minimum value - Matched card
	pair<Card*, int> min = *min_element(differences.begin(), differences.end(), pairCompare);
	this->leastDifferentCard = min.first;
}

Card* PlayedCard::getLeastDifferentCard() {
	return leastDifferentCard;
}

void PlayedCard::drawCardText(cv::Mat &srcImg) {

	int FONT_FACE = FONT_HERSHEY_TRIPLEX;
	int FONT_SCALE = 3;
	int FONT_THICKNESS = 5;
	int TOP_MARGIN = 80;

	Mat emptyCard = Mat::zeros(450, 450, srcImg.type());
	Mat emptyImg = Mat::zeros(srcImg.size(), srcImg.type());

	/*********************************************Top center text*********************************************/
	string cardName = this->getLeastDifferentCard()->getCard();
	if (cardName != "Joker")
		cardName += " " + this->getLeastDifferentCard()->getSuit();
	

	Size cardNameSize = getTextSize(cardName, FONT_FACE, FONT_SCALE, FONT_THICKNESS, 0);

	Point topCenterPoint = Point(225 - cardNameSize.width / 2, TOP_MARGIN);

	putText(emptyCard, cardName, topCenterPoint, FONT_FACE, FONT_SCALE, Scalar(255, 255, 255), FONT_THICKNESS);

	/***************************************************************************************************/

	/*********************************************Center text*********************************************/
	
	string result;
	Scalar color;
	if (this->result == 'w') {
		result = "Winner";
		color = Scalar(0, 255, 0);
	}
	else if(this->result == 'l') { 
		result = "Loser"; 
		color = Scalar(0, 0, 255);
	}
	else if (this->result == 'd') {
		result = "Draw";
		color = Scalar(255, 255,  0);
	}

	Size resultSize = getTextSize(result, FONT_FACE, FONT_SCALE, FONT_THICKNESS, 0);

	Point centerPoint = Point(225 - resultSize.width / 2, 225 + resultSize.height / 2);


	putText(emptyCard, result, centerPoint, FONT_FACE, FONT_SCALE, color, FONT_THICKNESS);

	/***************************************************************************************************/

	
	Point2f outputQuad[4] = { Point2f(0, 0), Point2f(450 - 1, 0), Point2f(450 - 1, 450 - 1), Point2f(0, 450 - 1) };
	Point2f cornerArray[4];

	for (size_t i = 0; i < 4; i++) {
		cornerArray[i] = this->cornerPoints[i];
	}

	Mat lambda = getPerspectiveTransform(outputQuad, cornerArray);
	warpPerspective(emptyCard, emptyImg, lambda, srcImg.size());

	overlapImages(srcImg, emptyImg);
}

void PlayedCard::overlapImages(Mat &image1, Mat image2) {
	for (int i = 0; i < image2.size().height; i++) {
		for (int j = 0; j < image2.size().width; j++) {
			Vec3b pixel = image2.at<Vec3b>(i, j);

			if (pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0) {
				image1.at<Vec3b>(i, j) = pixel;
			}
		}
	}
}

void PlayedCard::setWinner(char result){
	this->result = result;
}

char PlayedCard::getWinner(){
	return this->result;
}

PlayedCard::~PlayedCard() 
{}
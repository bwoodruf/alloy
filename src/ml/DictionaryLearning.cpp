/*
 * FilterBankLearning.cpp
 *
 *  Created on: Aug 27, 2017
 *      Author: blake
 */

#include "AlloyImageProcessing.h"
#include "AlloyAnisotropicFilter.h"
#include <AlloyOptimization.h>
#include <AlloyUnits.h>
#include <AlloyVolume.h>
#include <set>
#include <cereal/archives/xml.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/archives/portable_binary.hpp>
#include "ml/DictionaryLearning.h"
namespace aly {
void OrientedPatch::sample(const aly::Image1f& gray) {
	float2 tanget = MakeOrthogonalComplement(normal);
	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			float2 pix(width * (i / (float) (width - 1) - 0.5f),
					height * (j / (float) (height - 1) - 0.5f));
			data[i + j * width] = gray(position.x + dot(normal, pix),
					position.y + dot(tanget, pix));
		}
	}
}
float OrientedPatch::error(const std::vector<FilterBank>& banks) {
	int N = (int) data.size();
	int M = (int) weights.size();
	double err = 0;
	for (int i = 0; i < N; i++) {
		float val = 0;
		for (int j = 0; j < M; j++) {
			val += banks[j].data[i] * weights[j];
		}
		err += std::abs(data[i] - val);
	}
	err /= N;
	return (float) err;
}
float OrientedPatch::synthesize(const std::vector<FilterBank>& banks,
		std::vector<float>& sample) {
	int N = (int) data.size();
	int M = (int) weights.size();
	double err = 0;
	sample.resize(width*height);
	Image1f tmp(width,height);
	for (int i = 0; i < N; i++) {
		float val = 0;
		for (int j = 0; j < M; j++) {
			val += banks[j].data[i] * weights[j];
		}
		err += std::abs(data[i] - val);
		tmp[i]=clamp(val,0.0f,1.0f);
	}
	float2 norm =   float2(normal.x,-normal.y);
	float2 tanget = float2(normal.y, normal.x);
	for (int j = 0; j < width; j++) {
		for (int i = 0; i < height; i++) {
			float2 pix(width * (i / (float) (width - 1) - 0.5f),height * (j / (float) (height - 1) - 0.5f));
			pix=float2(width*0.5f+dot(norm, pix),height*0.5f+dot(tanget, pix));
			sample[i + j * width]=tmp(pix).x;
		}
	}
	err/=N;
	return (float) err;
}
void OrientedPatch::resize(int w, int h) {
	data.resize(w * h, 0.0f);
	width = w;
	height = h;
}
double FilterBank::score(const std::vector<float>& sample,bool correlation) {
	int N = (int) std::min(data.size(), sample.size());
	double err = 0;
	//use positive correlation to avoid cancellations
	if(correlation){
		for (int i = 0; i < N; i++) {
			err += data[i] * sample[i];
		}
		err= err/N;
		return -err;
	} else {
		for (int i = 0; i < N; i++) {
			err += std::abs(data[i] - sample[i]);
		}
		err /= N;
		return err;
	}
}

void FilterBank::normalize() {
	double sum=0.0;
	const int N=(int)data.size();
	for(int i=0;i<N;i++){
		sum+=data[i]*data[i];
	}
	sum=std::sqrt(sum)/N;
	if(sum>1E-10){
		for(int i=0;i<N;i++){
			data[i]/=sum;
		}
	}
}
DictionaryLearning::DictionaryLearning() {

}
std::set<int> DictionaryLearning::optimizeFilters(int start) {
	int S = patches.size();
	int K = filterBanks.size();
	int N = filterBanks.front().data.size();
	std::set<int> nonZeroSet;
	for (int s = 0; s < S; s++) {
		OrientedPatch& patch = patches[s];
		for (int k = start; k < K; k++) {
			float w = patch.weights[k];
			if (w != 0) {
				nonZeroSet.insert(k);
			}
		}
	}
	std::vector<int> nonZeroIndexes(nonZeroSet.begin(), nonZeroSet.end());
	std::sort(nonZeroIndexes.begin(), nonZeroIndexes.end());
	int KK = nonZeroIndexes.size();
	DenseMat<float> A(S, KK);
	Vec<float> AtB(KK);
	Vec<float> x(KK);
	for (int s = 0; s < S; s++) {
		OrientedPatch& patch = patches[s];
		for (int kk = 0; kk < KK; kk++) {
			int k = nonZeroIndexes[kk];
			float w = patch.weights[k];
			A[s][kk] = w;
		}
	}
	DenseMat<float> At = A.transpose();
	DenseMat<float> AtA = At * A;
	double totalRes = 0.0f;
	for (int n = 0; n < N; n++) {
		AtB.setZero();
		for (int kk = 0; kk < KK; kk++) {
			int k = nonZeroIndexes[kk];
			FilterBank& bank = filterBanks[k];
			x[kk] = bank.data[n];
			for (int s = 0; s < S; s++) {
				OrientedPatch& patch = patches[s];
				float val = patch.data[n];
				for (int ll = 0; ll < start; ll++) {
					val -= filterBanks[ll].data[n] * patch.weights[ll];
				}
				AtB[kk] += val * At[kk][s];
			}
		}
		x = Solve(AtA, AtB, MatrixFactorization::SVD);
		for (int kk = 0; kk < KK; kk++) {
			int k = nonZeroIndexes[kk];
			FilterBank& bank = filterBanks[k];
			bank.data[n] = x[kk];
		}
	}
	for (int kk = 0; kk < KK; kk++) {
		int k = nonZeroIndexes[kk];
		filterBanks[k].normalize();
	}
	for (int i = 0; i < start; i++) {
		nonZeroSet.insert(i);
	}
	return nonZeroSet;
}
void DictionaryLearning::solveOrthoMatchingPursuit(int T,
		OrientedPatch& patch) {
	std::vector<float>& sample = patch.data;
	std::vector<float>& weights = patch.weights;
	weights.resize(filterBanks.size(),0.0f);
	size_t B = filterBanks.size();
	size_t N = sample.size();
	DenseMat<float> A;
	Vec<float> b(N);
	Vec<float> x(N);
	Vec<float> r(N);
	double score;
	double bestScore = 1E30;
	int bestBank = -1;
	for (int n = 0; n < N; n++) {
		b.data[n] = sample[n];
	}
	std::vector<bool> mask(B, false);
	for (int bk = 0; bk < B; bk++) {
		FilterBank& bank = filterBanks[bk];
		score = bank.score(sample,true);
		if (score < bestScore) {
			bestScore = score;
			bestBank = bk;
		}
	}
	if (bestBank < 0) {
		return;
	}
	mask[bestBank] = true;
	int nonzero = 1;
	int count = 0;
	for (int t = 0; t < T; t++) {
		A.resize(N, nonzero);
		count = 0;
		for (int bk = 0; bk < B; bk++) {
			if (mask[bk]) {
				FilterBank& bank = filterBanks[bk];
				for (int j = 0; j < N; j++) {
					A[j][count] = bank.data[j];
				}
				count++;
			}
		}
		x = Solve(A, b, MatrixFactorization::SVD);
		r = A * x - b;
		bestScore = 1E30;
		count = 0;

		for (int bk = 0; bk < B; bk++) {
			if (mask[bk]) {
				weights[bk] = x[count++];
			} else {
				weights[bk] = 0.0f;
			}
		}
		bestBank = -1;
		if (t < T - 1) {
			for (int bk = 0; bk < B; bk++) {
				if (mask[bk]) {
					continue;
				}
				FilterBank& bank = filterBanks[bk];
				score = bank.score(r.data,true);
				if (score < bestScore) {
					bestScore = score;
					bestBank = bk;
				}
			}
			if (bestBank < 0) { // || bestScore < 1E-5f
				break;
			}
			mask[bestBank] = true;
			nonzero++;
		}
		//std::cout<<t<<") Weights "<<weights<<" residual="<<length(r)<<std::endl;
	}
}
void DictionaryLearning::optimizeWeights(int t) {
#pragma omp parallel for
	for (int idx = 0; idx < (int) patches.size(); idx++) {
		OrientedPatch& p = patches[idx];
		solveOrthoMatchingPursuit(t, p);
	}
}
double DictionaryLearning::error() {
	double err = 0;
	for (OrientedPatch& p : patches) {
		err += p.error(filterBanks);
	}
	err /= patches.size();
	return err;
}
void DictionaryLearning::add(const std::vector<FilterBank>& banks) {
	for (int b = 0; b < banks.size(); b++) {
		filterBanks.push_back(banks[b]);
		for (int n = 0; n < patches.size(); n++) {
			patches[n].weights.push_back(0.0f);
		}
	}
}
void DictionaryLearning::add(const FilterBank& banks) {
	filterBanks.push_back(banks);
	for (int n = 0; n < patches.size(); n++) {
		patches[n].weights.push_back(0.0f);
	}
}
void DictionaryLearning::removeFilterBanks(const std::set<int>& nonZeroSet) {
	int K = filterBanks.size();
	if (K == nonZeroSet.size())
		return;
	int count = 0;
	std::vector<FilterBank> newFilterBanks;
	for (int k = 0; k < K; k++) {
		if (nonZeroSet.find(k) != nonZeroSet.end()) {
			newFilterBanks.push_back(filterBanks[k]);
		} else {
			std::cout << "Removed Filter Bank " << k
					<< " because it's not used. " << std::endl;
		}
	}
	filterBanks = newFilterBanks;
	for (int n = 0; n < patches.size(); n++) {
		std::vector<float> newWeights;
		OrientedPatch& patch = patches[n];
		for (int k = 0; k < K; k++) {
			if (nonZeroSet.find(k) != nonZeroSet.end()) {
				newWeights.push_back(patch.weights[k]);
			}
		}
		patch.weights = newWeights;
	}
}
void WriteFilterBanksToFile(const std::string& file,
		std::vector<FilterBank>& params) {
	std::string ext = GetFileExtension(file);
	if (ext == "json") {
		std::ofstream os(file);
		cereal::JSONOutputArchive archive(os);
		archive(cereal::make_nvp("filter_banks", params));
	} else if (ext == "xml") {
		std::ofstream os(file);
		cereal::XMLOutputArchive archive(os);
		archive(cereal::make_nvp("filter_banks", params));
	} else {
		std::ofstream os(file, std::ios::binary);
		cereal::PortableBinaryOutputArchive archive(os);
		archive(cereal::make_nvp("filter_banks", params));
	}
}
void ReadFilterBanksFromFile(const std::string& file,
		std::vector<FilterBank>& params) {
	std::string ext = GetFileExtension(file);
	if (ext == "json") {
		std::ifstream os(file);
		cereal::JSONInputArchive archive(os);
		archive(cereal::make_nvp("filter_banks", params));
	} else if (ext == "xml") {
		std::ifstream os(file);
		cereal::XMLInputArchive archive(os);
		archive(cereal::make_nvp("filter_banks", params));
	} else {
		std::ifstream os(file, std::ios::binary);
		cereal::PortableBinaryInputArchive archive(os);
		archive(cereal::make_nvp("filter_banks", params));
	}
}

void DictionaryLearning::write(const std::string& outFile) {
	WriteFilterBanksToFile(outFile, filterBanks);
}
void DictionaryLearning::read(const std::string& outFile) {
	ReadFilterBanksFromFile(outFile, filterBanks);
}
void WritePatchesToFile(const std::string& file,const std::vector<OrientedPatch>& patches){
	std::vector<ImageRGB> tiles(patches.size());
	int M=std::ceil(std::sqrt(patches.size()));
	int N=(patches.size()%M==0)?(patches.size()/M):(1+patches.size()/M);
	for (size_t idx = 0; idx < patches.size(); idx++) {
		const OrientedPatch& p = patches[idx];
		ImageRGB& tile = tiles[idx];
		tile.resize(p.width, p.height);
		int count=0;
		for (int j = 0; j < p.height; j++) {
			for (int i = 0; i < p.width; i++) {
				tile[count] = ColorMapToRGB(p.data[count],ColorMap::RedToBlue);
				count++;
			}
		}
	}
	ImageRGB out;
	Tile(tiles, out, N, M);
	WriteImageToFile(file, out);
}
void DictionaryLearning::writeEstimatedPatches(const std::string& outImage,int M,int N) {
	//int M=std::ceil(std::sqrt(patches.size()));
	//int N=(patches.size()%M==0)?(patches.size()/M):(1+patches.size()/M);
	std::vector<ImageRGB> tiles(patches.size());
	std::vector<double> scores(patches.size());
	double maxError = 0;
	for (int n = 0; n < patches.size(); n++) {
		scores[n] = patches[n].error(filterBanks);
		if (scores[n] > maxError) {
			maxError = scores[n];
		}
	}

	for (size_t idx = 0; idx < patches.size(); idx++) {
		OrientedPatch& p = patches[idx];
		std::vector<float> tmp;
		p.synthesize(filterBanks, tmp);
		ImageRGB& tile = tiles[idx];
		tile.resize(p.width, p.height);
		int count = 0;
		float cw = scores[idx] / maxError;
		for (int j = 0; j < tile.height; j++) {
			for (int i = 0; i < tile.width; i++) {
				float val = tmp[count];
				if(val<0){
					tile[count] =aly::RGB(0,0,0);
				} else {
					float err = std::abs(tmp[count] - p.data[count]);
					tile[count] = ToRGB(HSVtoRGBf(HSV(1.0f - 0.9f * cw, cw, val)));
				}
				count++;
			}
		}
	}
	std::cout<<"Tiles "<<tiles.size()<<std::endl;
	ImageRGB out;
	Tile(tiles, out, N, M);
	WriteImageToFile(outImage, out);
}
double DictionaryLearning::score(std::vector<std::pair<int, double>>& scores) {
	scores.resize(patches.size());
	for (int n = 0; n < patches.size(); n++) {
		scores[n]= {n,patches[n].error(filterBanks)};
	}
	std::sort(scores.begin(), scores.end(),
			[=](const std::pair<int,double>& a,const std::pair<int,double>& b) {
				return (a.second>b.second);
			});
	return scores.front().second;
}
void DictionaryLearning::writeFilterBanks(const std::string& outImage) {
	int patchWidth = filterBanks.front().width;
	int patchHeight = filterBanks.front().height;
	ImageRGB filters(patchWidth * filterBanks.size(), patchHeight);
	for (int n = 0; n < filterBanks.size(); n++) {
		FilterBank& b = filterBanks[n];
		float minV = 1E30f;
		float maxV = -1E30f;
		size_t K = b.data.size();
		for (int k = 0; k < K; k++) {
			minV = std::min(minV, b.data[k]);
			maxV = std::max(maxV, b.data[k]);
		}
		float diff = (maxV - minV > 1E-4f) ? 1.0f / (maxV - minV) : 1.0f;
		for (int k = 0; k < K; k++) {
			int i = k % b.width;
			int j = k / b.width;
			filters(i + n * b.width, j) = ColorMapToRGB(
					(filterBanks[n].data[k] - minV) * diff,
					ColorMap::RedToBlue);
		}
	}
	WriteImageToFile(outImage, filters);
}
void DictionaryLearning::train(const std::vector<ImageRGBA>& images,
		int targetFilterBankSize, int subsample, int patchWidth,
		int patchHeight,int sparsity) {
	aly::Image1f gray;
	Image1f gX, gY;
	patches.clear();
	std::cout << "Start Learning " << patches.size() << "..." << std::endl;
	for (int nn = 0; nn < images.size(); nn++) {
		const ImageRGBA& img = images[nn];
		ConvertImage(img, gray);
		Gradient5x5(gray, gX, gY);
		int N = img.height / subsample;
		int M = img.width / subsample;
		for (int n = 0; n < N; n++) {
			for (int m = 0; m < M; m++) {
				float2 center = float2(m * subsample + subsample * 0.5f, n * subsample + subsample * 0.5f);
				float2 normal(gX(center).x, gY(center).x);
				/*
				if (length(normal) < 1E-6f) {
					normal = float2(1, 0);
				} else {
					normal = normalize(normal);
				}
				*/
				normal = float2(1, 0);
				OrientedPatch p(center, normal, patchWidth, patchHeight);
				p.sample(gray);
				patches.push_back(p);
			}
		}
	}
	std::cout<<"Patches "<<patches.size()<<" "<<patchWidth<<" "<<patchHeight<<std::endl;
	WritePatchesToFile(MakeDesktopFile("tiles.png"),patches);
	const int initFilterBanks = 13;
	filterBanks.clear();
	filterBanks.reserve(initFilterBanks);
	size_t K = patchWidth * patchHeight;
	for (int n = 0; n < initFilterBanks; n++) {
		filterBanks.push_back(FilterBank(patchWidth, patchHeight));
	}
	float var = 0.4f * 0.4f;
	//float var2 = 0.4f * 0.4f;
	//float var3 = 0.5f * 0.5f;

	float rcos=std::cos(ToRadians(8.0f));
	float rsin=std::sin(ToRadians(8.0f));

	for (int k = 0; k < K; k++) {
		int i = k % patchWidth;
		int j = k / patchWidth;
		float t = 2 * (j - (patchHeight - 1) * 0.5f) / (patchHeight - 1);
		float s = 2 * (i - (patchWidth - 1) * 0.5f) / (patchWidth - 1);
		filterBanks[0].data[k] = 1.0f;
		filterBanks[1].data[k] = s;
		filterBanks[2].data[k] = t;
		filterBanks[3].data[k] = 0.5f - 1.0f / (1 + std::exp(6 * s));
		filterBanks[4].data[k] = std::exp(-0.5f * s * s / var);

		float r1=rcos*s+rsin*t;
		float r2=rcos*s-rsin*t;
		filterBanks[5].data[k] = 0.5f - 1.0f / (1 + std::exp(6 * r1));
		filterBanks[6].data[k] = std::exp(-0.5f * r1 * r1 / var);
		filterBanks[7].data[k] = 0.5f - 1.0f / (1 + std::exp(6 * r2));
		filterBanks[8].data[k] = std::exp(-0.5f * r2 * r2 / var);
		//filterBanks[5].data[k] = 0.5f - 1.0f / (1 + std::exp(6 * s));
		//filterBanks[6].data[k] = std::exp(-0.5f * s * s / var2);
		//filterBanks[7].data[k] = 0.5f - 1.0f / (1 + std::exp(6 * s));
		//filterBanks[8].data[k] = std::exp(-0.5f * s * s / var3);

		filterBanks[9].data[k] = 0.5f - 1.0f / (1 + std::exp(6 * (s-0.333333f)));
		filterBanks[10].data[k] = std::exp(-0.5f * (s-0.333333f) * (s-0.333333f) / var);
		filterBanks[11].data[k] = 0.5f - 1.0f / (1 + std::exp(6 * (s+0.333333f)));
		filterBanks[12].data[k] = std::exp(-0.5f * (s+0.333333f) * (s+0.333333f) / var);
		//filterBanks[13].data[k] = 0.5f - 1.0f / (1 + std::exp(6 * (t-0.333333f)));
		//filterBanks[14].data[k] = std::exp(-0.5f * (t-0.333333f) * (t-0.333333f) / var);
		//filterBanks[15].data[k] = std::exp(-0.5f * (t * t + s * s) / var);

	}
	int KK = filterBanks.size() * 4;
//Orthogonal basis pursuit
	std::set<int> nonZeroSet;
	const int optimizationIterations = 4;
	int startFilter = filterBanks.size();
	for (int n = 0; n < filterBanks.size(); n++) {
		filterBanks[n].normalize();
	}
	//optimizeWeights(targetSparsity);
	for (int l = 0; filterBanks.size() < targetFilterBankSize; l++) {
		float err1=error() ;
		std::cout <<filterBanks.size()<<" Before Optimize Error: " << err1<<" for sparsity "<<sparsity<< std::endl;
		for (int h = 0; h < optimizationIterations; h++) {
			optimizeWeights(sparsity);
			nonZeroSet = optimizeFilters(startFilter);
		}
		float err2=error();
		std::cout <<filterBanks.size()<<" After Optimize Error: " << err2 <<" for sparsity "<<sparsity<<std::endl;
		/*
		std::cout<<"non-zero =[";
		for(int n:nonZeroSet){
			std::cout<<n<<" ";
		}

		std::cout<<"]"<<std::endl;
				*/
		if(std::abs(err2-err1)<1E-5f){
			sparsity++;
			startFilter--;
		}
		if (filterBanks.size() == targetFilterBankSize){
			break;
		}
		std::vector<std::pair<int, double>> scores;
		score(scores);
		//Add to filter bank patch with most error
		FilterBank fb(patches[scores[0].first].data, patchWidth, patchHeight);
		startFilter = filterBanks.size();
		add(fb);
		//filterBanks.back().normalize();
	}
}
}


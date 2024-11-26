#include <dirt/sampler.h>
#include <dirt/primetable.h>

namespace
{
  auto g_defaultSampler = std::make_shared<IndependentSampler>(json::object());
}

shared_ptr<Sampler> Sampler::defaultSampler()
{
  return g_defaultSampler;
}

void Sampler::startPixel()
{
  current1DDimension = 0;
  current2DDimension = 0;
  currentPixelSample = 0;
}

bool Sampler::startNextPixelSample()
{
  current1DDimension = 0;
  current2DDimension = 0;
  currentPixelSample++;
  currentGlobalSample++;
  return currentPixelSample < samplesPerPixel;
}

Vec2f Sampler::next2D()
{
  return Vec2f(next1D(), next1D());
}


PSSMLTSampler::PSSMLTSampler(float sigma, float largeStepProb, uint64_t seed): rng(seed,seed) {
    this->sigma = sigma;
    this->largeStepProb = largeStepProb;
}

int PSSMLTSampler::getNextIndex() {
    return sampleIndex++;
}

float PSSMLTSampler::next1D() {
    int index = getNextIndex();
    ensureReady(index);
    return X[index].val;
}

Vec2f PSSMLTSampler::next2D() {
    return Vec2f {next1D(),next1D()};
}

void PSSMLTSampler::startIter() {
    currentIter ++;
    sampleIndex = 0;
    isLargeStep = rng.nextFloat() < largeStepProb;
}

void PSSMLTSampler::accept() {
    if (isLargeStep){
        lastLargeStepIter = currentIter;
    }
}

void PSSMLTSampler::reject() {
    for(auto &Xi: X){
        if (Xi.lastModificationIter == currentIter){
            Xi.restore();
        }
    }
    --currentIter;
}


void PSSMLTSampler::ensureReady(int index) {
    if(index >= X.size()){
        X.resize(index + 1);
    }
    PrimarySample &Xi = X[index];

    // Make up for what we did not perturb as large step
    if(Xi.lastModificationIter < lastLargeStepIter){
        Xi.val = rng.nextFloat();
        Xi.lastModificationIter = lastLargeStepIter;
    }

    Xi.backup();
    if(isLargeStep){
        Xi.val = rng.nextFloat();
    }else{
        int nSmall = currentIter - Xi.lastModificationIter;
        float sampleVal = sqrt2 * erfInv(rng.nextFloat() * 2 - 1);
        float tmpSigma = sigma * std::sqrt((float)nSmall);
        Xi.val += sampleVal * tmpSigma;
        Xi.val -= std::floor(Xi.val);
    }
    Xi.lastModificationIter  =  currentIter;
}


/* The estimation of error function, from
 * Winitzki, Sergei. "A handy approximation for the error function and its inverse."
 * A lecture note obtained through private communication (2008).
 */
float PSSMLTSampler::erfInv(float x) {
    if (x==-1.0f)
        x = -0.9999f;
    float sign = x < 0.0f? -1.0f : 1.0f;
    float c = 2 / M_PI / 0.147;

    float x_tmp = (1 + x) * (1 - x);
    float ln_tmp = std::log(x_tmp);

    float t1 = c + ln_tmp * 0.5;
    float t2 = ln_tmp / 0.147;

    return sign * std::sqrt(-t1 + std::sqrt(t1 * t1 - t2));
}

bool PSSMLTSampler::isCurLargeStep() {
    return isLargeStep;
}



DRPSSMLTSampler::DRPSSMLTSampler(float largeStepProb, uint64_t seed, int maxDimension):
        maxDim(maxDimension),
        XProposedFirst(maxDimension), XProposedSecond(maxDimension), XCurrent(maxDimension),
        rng(seed,seed)
{

    this->largeStepProb = largeStepProb;
    this->isFirst = true;
    this->isLargeStep = true;
}

int DRPSSMLTSampler::getNextIndex() {
    return sampleIndex++;
}

float DRPSSMLTSampler::next1D() {
    int index = getNextIndex();
    return getPrimarySample(index);
}

Vec2f DRPSSMLTSampler::next2D() {
    return Vec2f {next1D(),next1D()};
}

//Fill space
void DRPSSMLTSampler::startIter() {
    currentIter ++;
    sampleIndex = 0;
    isLargeStep = rng.nextFloat() < largeStepProb;
}

void DRPSSMLTSampler::accept(bool acceptFirst) {

    XCurrent = acceptFirst ? XProposedFirst : XProposedSecond;

    sampleIndex = 0;
    isFirst = true;
    XProposedFirst.clear();
    XProposedSecond.clear();

}

void DRPSSMLTSampler::reject() {
    sampleIndex = 0;
    isFirst = true;
    XProposedFirst.clear();
    XProposedSecond.clear();
}

void DRPSSMLTSampler::nextStage() {
    sampleIndex = 0;
    isFirst = false;
}


/* The estimation of error function, from
 * Winitzki, Sergei. "A handy approximation for the error function and its inverse."
 * A lecture note obtained through private communication (2008).
 */
float DRPSSMLTSampler::erfInv(float x) {
    if (x==-1.0f)
        x = -0.9999f;
    float sign = x < 0.0f? -1.0f : 1.0f;
    float c = 2 / M_PI / 0.147;

    float x_tmp = (1 + x) * (1 - x);
    float ln_tmp = std::log(x_tmp);

    float t1 = c + ln_tmp * 0.5;
    float t2 = ln_tmp / 0.147;

    return sign * std::sqrt(-t1 + std::sqrt(t1 * t1 - t2));
}

bool DRPSSMLTSampler::isCurLargeStep() {
    return isLargeStep;
}

Vec2f DRPSSMLTSampler::mutationKelemen(Vec2f w) {
    float sample = rng.nextFloat();
    int sign;
    if (sample < 0.5){
        sign = 1;
        sample *= 2.0f;
    }else{
        sign = -1;
        sample = 2.0f * (sample - 0.5f);
    }
    float dv = s2_kelemen * std::exp(-std::log(s2_kelemen / s1_kelemen) * (1 - sample));
    dv = dv * sign;

    // dv is the d in Algorithm2

    float phi = M_PI * 2 * rng.nextFloat();

    Vec2f proposedVec{ w.x + dv * std::cos(phi), w.y + dv * std::sin(phi) };


    return proposedVec;
}


Vec2f DRPSSMLTSampler::mutationCauchy(Vec2f current, Vec2f proposed) {
    float du1 = proposed.x - current.x;
    float du2 = proposed.y - current.y;

    float sample = rng.nextFloat();
    int sign = 1;
    if(sample < 0.5f){
        sign = 1;
        sample *= 2.0f;
    }else{
        sign = -1;
        sample = 2.0f * (sample - 0.5f);
    }

    float v = std::cos(2.0 * M_PI * sample);
    float angle = (v + dispersion) / (1.0 + dispersion * v);
    float theta = sign * safe_acos(angle);

    float norm = std::sqrt(du1 * du1 + du2 * du2);
    float mu_i = safe_acos( -du1 / norm );
    if (-du2 < 0)
        mu_i = 2.0f * M_PI - mu_i;

    auto c1 = proposed.x + std::cos(theta + mu_i) * norm;
    auto c2 = proposed.y + std::sin(theta + mu_i) * norm;

    return Vec2f{c1,c2};
}

void DRPSSMLTSampler::fillPSSpace(bool isFirstStage) {
    auto &proposed = isFirstStage? XProposedFirst : XProposedSecond;
    // As this logic. If this iteration is large step. It is possible to generate two large step
    for (size_t i =0; i < maxDim; i++){
        if(isLargeStep){
            proposed.push_back(rng.nextFloat());
        }else if(isFirstStage){
            Vec2f val = mutationKelemen(Vec2f{XCurrent[i],XCurrent[i+1]});
            proposed.push_back(roundInOne(val.x));
            proposed.push_back(roundInOne(val.y));
            i++;
        }else{
            Vec2f cur{XCurrent[i],XCurrent[i+1]};
            Vec2f firstProposed{XProposedFirst[i],XProposedFirst[i+1]};
            Vec2f val = mutationCauchy(cur,firstProposed);
            proposed.push_back(roundInOne(val.x));
            proposed.push_back(roundInOne(val.y));
            i++;
        }
    }
}


float DRPSSMLTSampler::getPrimarySample(size_t k) {
    auto &proposed = isFirst? XProposedFirst:XProposedSecond;

    if(k==0){
        proposed.clear();
    }

    if(k==0){
        fillPSSpace(isFirst);
    }

    if(k > maxDim){
        throw DirtException("Dimension exceed maximum");
    }

    return proposed[k];
}

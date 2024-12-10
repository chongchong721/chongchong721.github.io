#pragma once

#include <dirt/parser.h>
#include <dirt/fwd.h>

class Sampler
{
public:
  // default to an independent sampler
	static shared_ptr<Sampler> defaultSampler();

  virtual ~Sampler() = default;

  /**
  *  Call when starting to evaluate a new pixel, resets various counters.
  *  Derived classes can override this function and use it to pre-generate samples for a pixel.
  */
  virtual void startPixel();

 /**
  *  Call when starting to evaluate a new sample for the same pixel, increments and resets various counters.
  */
  virtual bool startNextPixelSample();

  /**
   * Generate a single random number.
   */
  virtual float next1D() = 0;

  /**
  * Genearte a pair of random numbers. If a sampler does not provide a next2D(), the base clase
  * will call the derived class's next1D() twice and return the pair.
  * 
  * Allows the next1D() to increment the current1DDimension, doesn't touch current2DDimension
  */
  virtual Vec2f next2D();

  // the number of samples **of** each pixel
  size_t samplesPerPixel; 

  // the number of sampels **for** each pixel (used to sample camera offsets, directions, etc)
  size_t dimension;

protected:
  // how many samples **of** the current pixel have been evalated
  size_t currentPixelSample = 0;

  // how many samples of the entire image have been evaluated (in any pixel)
  size_t currentGlobalSample = 0;

  // how many 1D samples have been generated for the current pixel
  size_t current1DDimension = 0;

  // how many 2D samples have been generated for the current pixel
  size_t current2DDimension = 0;
};



class PSSMLTSampler: public Sampler{
public:
    PSSMLTSampler(const json &j);
    PSSMLTSampler(float sigma, float largeStepProb, uint64_t seed);

    // Called at each iteration start
    void startIter();
    void accept();
    void reject();
    float next1D() override;
    Vec2f next2D() override;
    bool isCurLargeStep();

private:

    struct PrimarySample{
        float val = 0; // Value of this random variable
        int lastModificationIter = 0; // Last iteration that the value was changed/used
        float valBackup = 0;
        int modificationIterBackup = 0;

        void backup(){
            valBackup = val;
            modificationIterBackup = lastModificationIter;
        }

        void restore(){
            val = valBackup;
            lastModificationIter = modificationIterBackup;
        }

    };
    float sigma; // parameter for local step
    float largeStepProb; // probability for large step to happen
    bool isLargeStep = true;

    int currentIter = 0;
    int lastLargeStepIter = 0;

    int sampleIndex = 0;

    std::vector<PrimarySample> X;

    pcg32 rng;

    const float sqrt2 = std::sqrt(2.0);

    int getNextIndex();
    void ensureReady(int index);



    // Estimation of the inverse of error function
    static float erfInv(float x);

};





class DRPSSMLTSampler: public Sampler{
public:
    DRPSSMLTSampler();
    DRPSSMLTSampler(const json &j);
    DRPSSMLTSampler(float largeStepProb, uint64_t seed, int maxDimension);

    // Called at each iteration start
    void startIter();
    void accept(bool acceptFirsts);
    void reject();
    float next1D() override;
    Vec2f next2D() override;
    bool isCurLargeStep();
    float getPrimarySample(size_t k);
    void nextStage();

private:

    float largeStepProb; // probability for large step to happen
    bool isLargeStep = true;

    int currentIter = 0;

    int sampleIndex = 0;


    size_t maxDim;

    static inline float safe_acos(float value){
        return std::acos(std::min(1.0f,std::max(-1.0f,value)));
    }

    // Kelemen style mutation - parameter
    const float _s1_kelemen = 1.0 / 1024.0;
    const float _s2_kelemen = 1.0 / 64.0;
    const float scale_kelemen = 1.9f; //what is this? To make the scaling the same in 2D space

    const float s1_kelemen = _s1_kelemen * scale_kelemen;
    const float s2_kelemen = _s2_kelemen * scale_kelemen;

    // Warped Cauchy - parameter
    const float rho = std::exp(-0.25f);
    const float dispersion = 2.0f * rho / (1.0 + rho * rho);

    // Proposed value
    std::vector<float> XProposedFirst;
    std::vector<float> XProposedSecond;

    // Current value
    std::vector<float> XCurrent;

    pcg32 rng;

    bool isFirst;

    int getNextIndex();



    // Estimation of the inverse of error function
    static float erfInv(float x);

    // Kelemen mutation
    Vec2f mutationKelemen(Vec2f w);

    // Cauchy 2D mutation
    Vec2f mutationCauchy(Vec2f ,Vec2f);

    // Fill the primary space
    void fillPSSpace(bool isFirstStage);

    float roundInOne(float x){
       return x - std::floor(x);
    }

};

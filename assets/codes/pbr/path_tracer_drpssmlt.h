//
// Created by kane on 5/3/23.
//

#ifndef DIRT_PATH_TRACER_DRPSSMLT_H
#define DIRT_PATH_TRACER_DRPSSMLT_H



//
// Created by kane on 4/26/23.
//

#pragma once

#include <dirt/integrator.h>
#include <dirt/scene.h>
#include <dirt/pdf.h>
#include <dirt/sampler.h>
#include <dirt/path_tracer_mixture.h>
#include <dirt/path_tracer_mis.h>

class DRPSSMLTIntegrator: public Integrator {
public:
    DRPSSMLTIntegrator(const json &j = json::object()) {
        largeStepProb = j.value("largestepprob", largeStepProb);
        nSampler = j.value("nsampler", nSampler);
        maxDimension = j.value("maxdim",80);
    }


    virtual Color3f Li_local(const Scene &scene, Sampler &sampler, Vec2i &filmLocation) {
        auto camera = scene.getCamera();
        float i = (float) camera->resolution().x * sampler.next1D();
        float j = (float) camera->resolution().y * sampler.next1D();

        if (i < 0.0f || i > camera->resolution().x || j < 0.0f || j > camera->resolution().y){
            filmLocation.x = -1;
            filmLocation.y = -1;
            throw DirtException("Out of range resolution");
            return Color3f {-1.0f};
        }

        filmLocation.x = std::floor(i);
        filmLocation.y = std::floor(j);



        if (filmLocation.x == camera->resolution().x) {
            filmLocation.x = camera->resolution().x - 1;
        }
        if (filmLocation.y == camera->resolution().y) {
            filmLocation.y = camera->resolution().y - 1;
        }

        Ray3f cameraRay = camera->generateRay(i, j);
        return internal_integrator->Li(scene, sampler, cameraRay);
    }

    virtual Image3f MLTRender(const Scene &scene, int imageSamples) override {
        auto camera = scene.getCamera();
        int area = camera->resolution().x * camera->resolution().y;
        int bootstrapNum = 100000;
        std::vector<float> bootstrapWeight(bootstrapNum, 0.0);

        float b = 0.0;

        for (int i = 0; i < bootstrapNum; i++) {
            uint64_t rng_idx = i;
            DRPSSMLTSampler sampler(largeStepProb, rng_idx, maxDimension);

            Vec2i filmLocation;

            auto result = Li_local(scene, sampler, filmLocation);
            if (isInvalid(luminance(result))){
                bootstrapWeight[rng_idx] = 0.0f;
            }else{
                bootstrapWeight[rng_idx] = luminance(result);
                b += luminance(result);
            }
        }

        b /= (float) bootstrapNum;

        std::vector<float> bootstrapWeightCdf(bootstrapNum, 0.0);
        float sum = 0.0;
        for (int i = 0; i < bootstrapNum; ++i) {
            sum += bootstrapWeight[i];
            bootstrapWeightCdf[i] = sum;
        }

        for (int i = 0; i < bootstrapNum; ++i) {
            bootstrapWeightCdf[i] /= sum;
        }

        //Array2d<float> tmp(camera->resolution().x, camera->resolution().y);
        auto image = Image3f(camera->resolution().x, camera->resolution().y);
        for (auto j: range(camera->resolution().y)) {
            for (auto i: range(camera->resolution().x)) {
                image(i, j) = Color3f(0.0f,0.0f,0.0f);
                //tmp(i, j) = 0.0f;
            }

        }


        int nSamples = scene.getImageSamples() * area;
        int nMutations = nSamples / nSampler;

        for (int i = 0; i < nSampler; ++i) {
            Color3f LCurrent;
            uint64_t rng_idx;
            Vec2i filmLocationCurrent;

            rng_idx = sampleFromDiscreteDistribution(bootstrapWeightCdf);
            DRPSSMLTSampler sampler(largeStepProb, rng_idx, maxDimension);
            LCurrent = Li_local(scene, sampler, filmLocationCurrent);
            //printf("Test rng_idx:%lu, luminance is %f\n",rng_idx, luminance(LCurrent));
            if(isInvalid(luminance(LCurrent))){
                // Make sure that Lcurrent won't return NaN
                i--;
                std::cout << "luminance test invalid" << std::endl;
                continue;
            }



            // A very strange bug that the Li value returned from same random number generator is different.

            sampler.accept(true); // Write the initialization large step to XCurrent
            INCREMENT_TRACED_RAYS;

            for (int j = 0; j < nMutations; ++j) {
                INCREMENT_TRACED_RAYS;
                sampler.startIter();

                std::pair<float,float>a = std::make_pair(0.0,0.0);
                std::pair<bool,bool>acceptStatus = std::make_pair(false, false);
                std::pair<float,float>proposedWeight = std::make_pair(0.0,0.0);

                Vec2i filmLocationProposedFirst;
                Vec2i filmLocationProposedSecond;
                Color3f LProposedSecond;
                Color3f LProposedFirst = Li_local(scene, sampler, filmLocationProposedFirst);

                if (isInvalid(luminance(LProposedFirst))){
                    a.first = 0.0f;
                    acceptStatus.first = false;
                }else{
                    a.first = std::min(1.0f, luminance(LProposedFirst) / luminance(LCurrent));
                    acceptStatus.first = flipCoin(a.first);
                }


                bool doSecond = false;
                doSecond = !acceptStatus.first;

                if(!timidAfterLarge){
                    doSecond = doSecond && !sampler.isCurLargeStep();
                }

                if (doSecond){
                    sampler.nextStage();
                    LProposedSecond = Li_local(scene,sampler,filmLocationProposedSecond);

                    if (isInvalid(luminance(LProposedSecond))){
                        a.second = 0.0f;
                        acceptStatus.second = false;
                    }else{
//                        if (isInvalidSpectrum(LProposedFirst)){
//                            throw DirtException("Spectrum has negative value");
//                        }
                        //Compute second acceptance rate
                        if (luminance(LProposedSecond) < luminance(LProposedFirst)){
                            a.second = 0.0f; acceptStatus.second= false;
                        }
                        else if(luminance(LProposedSecond) >= luminance(LCurrent)){
                            a.second = 1.0f; acceptStatus.second = true;
                        }else{
                            a.second = (luminance(LProposedSecond) - luminance(LProposedFirst)) / (luminance(LCurrent) -
                                                                                                   luminance(LProposedFirst));
                            acceptStatus.second = flipCoin(a.second);
                        }
                    }
                }

                proposedWeight.first = a.first;
                proposedWeight.second = (1.0f - a.first) * a.second;
                float currentWeight = 1.0 - proposedWeight.first - proposedWeight.second;

//                if(proposedWeight.first < -Epsilon || proposedWeight.second < -Epsilon || currentWeight < -Epsilon){
//                    throw DirtException("weight less than 0");
//                }

                if(proposedWeight.first > 0.0){
                    auto contribution = LProposedFirst * proposedWeight.first / luminance(LProposedFirst);
//                    if(isInvalidSpectrum(contribution)){
//                        printf("j=%d, L1 is [%f,%f,%f] weight is %f, luminance is %f\n",j,LProposedFirst.x,
//                               LProposedFirst.y,LProposedFirst.z,proposedWeight.first, luminance(LProposedFirst));
//                        throw DirtException("Invalid Spectrum");
//                    }
                    image(filmLocationProposedFirst.x, filmLocationProposedFirst.y) += contribution;
                }
                // Need to check due to NaN error
                if(proposedWeight.second > 0.0){
                    auto contribution = LProposedSecond * proposedWeight.second / luminance(LProposedSecond);
//                    if(isInvalidSpectrum(contribution)){
//                        printf("j=%d, L2 is [%f,%f,%f] weight is %f, luminance is %f\n",j,LProposedSecond.x,
//                               LProposedSecond.y,LProposedSecond.z,proposedWeight.second, luminance(LProposedSecond));
//                        throw DirtException("Invalid Spectrum");
//                    }
                    image(filmLocationProposedSecond.x, filmLocationProposedSecond.y) += contribution;
                }

                if(currentWeight > 0.0){
                    auto contribution = LCurrent * currentWeight / luminance(LCurrent);
//                    if(isInvalidSpectrum(contribution)){
//                        printf("j=%d LC is [%f,%f,%f] weight is %f, luminance is %f\n",j,LCurrent.x,
//                               LCurrent.y,LProposedFirst.z,currentWeight, luminance(LCurrent));
//                        throw DirtException("Invalid Spectrum");
//                    }
                    image(filmLocationCurrent.x, filmLocationCurrent.y) += contribution;
                }


                if(acceptStatus.first || acceptStatus.second){
                    if(acceptStatus.first){
                        LCurrent = LProposedFirst;
                        filmLocationCurrent = filmLocationProposedFirst;
                    }else{
                        LCurrent = LProposedSecond;
                        filmLocationCurrent = filmLocationProposedSecond;
                    }
                    sampler.accept(acceptStatus.first);
                }
                    // Reject the first proposal
                else{
                    sampler.reject();
                }

            }

        }

        for (auto j: range(camera->resolution().y)) {
            for (auto i: range(camera->resolution().x)) {
                image(i, j) *= (b / scene.getImageSamples());
            }
        }

//        for (auto j: range(camera->resolution().y)) {
//            for (auto i: range(camera->resolution().x)) {
//                printf("Pixel[%d,%d]:[%f,%f,%f]\n",i,j,image(i,j).x,image(i,j).y,image(i,j).z);
//            }
//        }

        return image;

    }


        virtual Image3f MLTRenderTime(const Scene &scene, int imageSamples)
        override{

            int scene_size = 50;
            // Create pre-computed scene-list
            std::vector<Scene> sceneList(scene_size + 1);
            for ( int i = 0; i < scene_size + 1; ++i){
                float rate = (float) i / (float) scene_size;
                sceneList[i] = Scene(scene.json1, rate);
            }

            auto camera = scene.getCamera();
            int area = camera->resolution().x * camera->resolution().y;
            int bootstrapNum = 200000;
            std::vector<float> bootstrapWeight(bootstrapNum, 0.0);

            float b = 0.0;

            for (int i = 0; i < bootstrapNum; i++) {
                uint64_t rng_idx = i;
                DRPSSMLTSampler sampler(largeStepProb, rng_idx, maxDimension);

                Vec2i filmLocation;

                auto result = Li_local(scene, sampler, filmLocation);
                if (isInvalid(luminance(result))){
                    bootstrapWeight[rng_idx] = 0.0f;
                }else{
                    bootstrapWeight[rng_idx] = luminance(result);
                    b += luminance(result);
                }
            }

            b /= (float) bootstrapNum;

            std::vector<float> bootstrapWeightCdf(bootstrapNum, 0.0);
            float sum = 0.0;
            for (int i = 0; i < bootstrapNum; ++i) {
                sum += bootstrapWeight[i];
                bootstrapWeightCdf[i] = sum;
            }

            for (int i = 0; i < bootstrapNum; ++i) {
                bootstrapWeightCdf[i] /= sum;
            }

            //Array2d<float> tmp(camera->resolution().x, camera->resolution().y);
            auto image = Image3f(camera->resolution().x, camera->resolution().y);
            for (auto j: range(camera->resolution().y)) {
                for (auto i: range(camera->resolution().x)) {
                    image(i, j) = Color3f(0.0f);
                    //tmp(i, j) = 0.0f;
                }

            }


            int nSamples = scene.getImageSamples() * area;
            int nMutations = nSamples / nSampler;

            for (int i = 0; i < nSampler; ++i) {
                int rng_idx = sampleFromDiscreteDistribution(bootstrapWeightCdf);
                DRPSSMLTSampler sampler(largeStepProb, rng_idx, maxDimension);

                float u = sampler.next1D();
                int idx = static_cast<int>(std::round(u * (float) scene_size));

                Vec2i filmLocationCurrent;
                Color3f LCurrent = Li_local(sceneList[idx], sampler, filmLocationCurrent);

                if(isInvalid(luminance(LCurrent))){
                    i--;
                    continue;
                }

                sampler.accept(true); // Write the initialization large step to XCurrent
                INCREMENT_TRACED_RAYS;

                for (int j = 0; j < nMutations; ++j) {
                    INCREMENT_TRACED_RAYS;
                    sampler.startIter();

                    u = sampler.next1D();
                    idx = static_cast<int>(std::round(u * (float) scene_size));

                    std::pair<float,float>a = std::make_pair(0.0,0.0);
                    std::pair<bool,bool>acceptStatus = std::make_pair(false, false);
                    std::pair<float,float>proposedWeight = std::make_pair(0.0,0.0);

                    Vec2i filmLocationProposedFirst;
                    Vec2i filmLocationProposedSecond;
                    Color3f LProposedSecond;
                    Color3f LProposedFirst = Li_local(sceneList[idx], sampler, filmLocationProposedFirst);

                    if (isInvalid(luminance(LProposedFirst))){
                        a.first = 0.0f;
                        acceptStatus.first = false;
                    }else{
                        a.first = std::min(1.0f, luminance(LProposedFirst) / luminance(LCurrent));
                        acceptStatus.first = flipCoin(a.first);
                    }


                    bool doSecond = false;
                    doSecond = !acceptStatus.first;

                    if(!timidAfterLarge){
                        doSecond = doSecond && !sampler.isCurLargeStep();
                    }

                    if (doSecond){
                        sampler.nextStage();
                        LProposedSecond = Li_local(scene,sampler,filmLocationProposedSecond);

                        if (isInvalid(luminance(LProposedSecond))){
                            a.second = 0.0f;
                            acceptStatus.second = false;
                        }else{
                            //Compute second acceptance rate
                            if (luminance(LProposedSecond) < luminance(LProposedFirst)){
                                a.second = 0.0f; acceptStatus.second= false;
                            }
                            else if(luminance(LProposedSecond) >= luminance(LCurrent)){
                                a.second = 1.0f; acceptStatus.second = true;
                            }else{
                                a.second = (luminance(LProposedSecond) - luminance(LProposedFirst)) / (luminance(LCurrent) -
                                                                                                       luminance(LProposedFirst));
                                acceptStatus.second = flipCoin(a.second);
                            }
                        }
                    }

                    proposedWeight.first = a.first;
                    proposedWeight.second = (1.0f - a.first) * a.second;
                    float currentWeight = 1.0 - proposedWeight.first - proposedWeight.second;

                    if(proposedWeight.first!=0.0)
                        image(filmLocationProposedFirst.x, filmLocationProposedFirst.y) += LProposedFirst * proposedWeight.first / luminance(LProposedFirst);
                    // Need to check due to NaN error
                    if(proposedWeight.second!=0.0)
                        image(filmLocationProposedSecond.x, filmLocationProposedSecond.y) += LProposedSecond * proposedWeight.second / luminance(LProposedSecond);
                    if(currentWeight!=0.0)
                        image(filmLocationCurrent.x, filmLocationCurrent.y) += LCurrent * currentWeight / luminance(LCurrent);

                    if(acceptStatus.first || acceptStatus.second){
                        if(acceptStatus.first){
                            LCurrent = LProposedFirst;
                            filmLocationCurrent = filmLocationProposedFirst;
                        }else{
                            LCurrent = LProposedSecond;
                            filmLocationCurrent = filmLocationProposedSecond;
                        }
                        sampler.accept(acceptStatus.first);
                    }
                        // Reject the first proposal
                    else{
                        sampler.reject();
                    }

                }

            }

            for (auto j: range(camera->resolution().y)) {
                for (auto i: range(camera->resolution().x)) {
                    image(i, j) *= (b / scene.getImageSamples());
                }
            }
            return image;
        }


    virtual bool isMLTIntegrator()
    override{
        return true;
    }

    std::shared_ptr<Integrator> internal_integrator;

private:
    uint64_t sampleFromDiscreteDistribution(const std::vector<float> &cdf) {
        auto size = cdf.size();

        float u = randf();

        auto low = std::lower_bound(cdf.begin(), cdf.end(), u);
        uint64_t idx = low - cdf.begin();
        return idx;
    }

    bool flipCoin(float x){
        return randf() < x;
    }

    bool isInvalidInit(float x){
        return std::isnan(x) || std::isinf(x) || x < 0.0f ;
    }

    bool isInvalid(float x){
        return std::isnan(x) || std::isinf(x) || x <= 0.0001f ;
    }

    bool isInvalidSpectrum(Color3f x){
        if (std::isnan(x.x) || std::isnan(x.y) || std::isnan(x.z) )
            return true;
        else
            return false;
    }


    bool timidAfterLarge = false; // If true, second stage happened after large step & Kelemen mutation. if False, only after Kelemen
    int maxDimension;
    float largeStepProb;
    int nSampler; // number of sampler used in MCMC iteration
};







#endif //DIRT_PATH_TRACER_DRPSSMLT_H

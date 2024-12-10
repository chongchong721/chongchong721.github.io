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

class PSSMLTIntegrator: public Integrator {
public:
    PSSMLTIntegrator(const json &j = json::object()) {
        sigma = j.value("sigma", sigma);
        largeStepProb = j.value("largestepprob", largeStepProb);
        nSampler = j.value("nsampler", nSampler);
    }


    virtual Color3f Li_local(const Scene &scene, Sampler &sampler, Vec2i &filmLocation) {
        auto camera = scene.getCamera();
        float i = (float) camera->resolution().x * sampler.next1D();
        float j = (float) camera->resolution().y * sampler.next1D();

        filmLocation.x = std::floor(i);
        filmLocation.y = std::floor(j);
        if (filmLocation.x >= camera->resolution().x) {
            filmLocation.x = camera->resolution().x - 1;
        }
        if (filmLocation.y >= camera->resolution().y) {
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
            PSSMLTSampler sampler(sigma, largeStepProb, rng_idx);

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

        Array2d<float> tmp(camera->resolution().x, camera->resolution().y);
        auto image = Image3f(camera->resolution().x, camera->resolution().y);
        for (auto j: range(camera->resolution().y)) {
            for (auto i: range(camera->resolution().x)) {
                image(i, j) = Color3f(0.0f);
                tmp(i, j) = 0.0f;
            }

        }


        int nSamples = scene.getImageSamples() * area;
        int nMutations = nSamples / nSampler;
        //float M = std::floor(1.0f / (1.0f + largeStepProb));
        bool useMis = true;

        for (int i = 0; i < nSampler; ++i) {
            int rng_idx = sampleFromDiscreteDistribution(bootstrapWeightCdf);
            PSSMLTSampler sampler(sigma, largeStepProb, rng_idx);

            Vec2i filmLocationCurrent;
            Color3f LCurrent = Li_local(scene, sampler, filmLocationCurrent);

            if(isInvalid(luminance(LCurrent))){
                i--;
                continue;
            }
            INCREMENT_TRACED_RAYS;

            for (int j = 0; j < nMutations; ++j) {
                INCREMENT_TRACED_RAYS;
                sampler.startIter();
                Vec2i filmLocationProposed;
                Color3f LProposed = Li_local(scene, sampler, filmLocationProposed);

                float accept = std::min(1.0f, luminance(LProposed) / luminance(LCurrent));

                bool LproposedInvalid = isInvalid(luminance(LProposed));

                if (LproposedInvalid){
                    accept = 0.0f;
                    image(filmLocationCurrent.x, filmLocationCurrent.y) +=
                            LCurrent*  (1.0f - accept) / ( luminance(LCurrent) / b + largeStepProb);
                }else{
                    if (useMis) {
                        if (accept > 0.0f) {
                            image(filmLocationProposed.x, filmLocationProposed.y) +=
                                    LProposed *  (accept + (sampler.isCurLargeStep() ? 1.0f : 0.0f)) /
                                    (luminance(LProposed) / b + largeStepProb) ;

                        }
                        image(filmLocationCurrent.x, filmLocationCurrent.y) +=
                                LCurrent*  (1.0f - accept) / ( luminance(LCurrent) / b + largeStepProb);
                    }else{
                        if (accept > 0.0f){
                            image(filmLocationProposed.x,filmLocationProposed.y) += LProposed * accept / luminance(LProposed);
                        }
                        image(filmLocationCurrent.x,filmLocationCurrent.y) += LCurrent * (1.0f - accept) / luminance(LCurrent);
                    }
                }




                float u = randf();
                if (u < accept) {
                    filmLocationCurrent = filmLocationProposed;
                    LCurrent = LProposed;
                    sampler.accept();
                } else {
                    sampler.reject();
                }

            }

        }

        for (auto j: range(camera->resolution().y)) {
            for (auto i: range(camera->resolution().x)) {
                image(i, j) *= ((useMis ? 1.0f : b) / scene.getImageSamples());
            }
        }
        return image;

    }


        virtual Image3f MLTRenderTime(const Scene &scene, int imageSamples)
        override{
                bool useMis = true;
                float time = scene.m_time;

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

                for (int i = 0; i < bootstrapNum; i++){
                    uint64_t rng_idx = i;
                    PSSMLTSampler sampler(sigma, largeStepProb, rng_idx);

                    float u = sampler.next1D();

                    int idx = static_cast<int>(std::round(u * (float) scene_size));


                    Vec2i filmLocation;

                    auto result = Li_local(sceneList[idx], sampler, filmLocation);
                    if (isInvalid(luminance(result))){
                        bootstrapWeight[rng_idx] = 0.0f;
                    }else{
                        bootstrapWeight[rng_idx] = luminance(result);
                        b += luminance(result);
                    }
                }

                b /= (float)bootstrapNum;
                std::cout << "b value is " << b << std::endl;

                std::vector<float> bootstrapWeightCdf(bootstrapNum, 0.0);
                float sum = 0.0;
                for (int i = 0; i < bootstrapNum; ++i){
                    sum += bootstrapWeight[i];
                    bootstrapWeightCdf[i] = sum;
                }

                for (int i =0; i < bootstrapNum; ++i){
                    bootstrapWeightCdf[i] /= sum;
                }


                auto image = Image3f(camera->resolution().x, camera->resolution().y);
                for (auto j : range(camera->resolution().y)){
                    for (auto i: range(camera->resolution().x))
                        image(i, j) = Color3f(0.0f);
                }


                int nSamples = scene.getImageSamples() * area;
                int nMutations = nSamples / nSampler;

                for (int i = 0; i < nSampler; ++i){
                    int rng_idx = sampleFromDiscreteDistribution(bootstrapWeightCdf);
                    printf("RNG_IDX is %d\n", rng_idx);
                    PSSMLTSampler sampler(sigma, largeStepProb, rng_idx);

                    float u = sampler.next1D();
                    int idx = static_cast<int>(std::round(u * (float) scene_size));


                    Vec2i filmLocationCurrent;
                    Color3f LCurrent = Li_local(sceneList[idx], sampler, filmLocationCurrent);

                    if(isInvalid(luminance(LCurrent))){
                        i--;
                        continue;
                    }
                    INCREMENT_TRACED_RAYS;

                    for (int j = 0; j < nMutations; ++j) {
                        INCREMENT_TRACED_RAYS;
                        sampler.startIter();

                        u = sampler.next1D();
                        idx = static_cast<int>(std::round(u * (float) scene_size));


                        Vec2i filmLocationProposed;
                        Color3f LProposed = Li_local(sceneList[idx], sampler, filmLocationProposed);
                        float accept = 0.0f;
                        bool inValidProposal = isInvalid(luminance(LProposed));


                        if(inValidProposal){
                            accept = 0.0f;
                            image(filmLocationCurrent.x, filmLocationCurrent.y) +=
                                    LCurrent * (1.0f - accept) / luminance(LCurrent);
                        }
                        else{
                            accept = std::min(1.0f, luminance(LProposed) / luminance(LCurrent));
                            if (useMis) {
                                if (accept > 0.0f) {
                                    image(filmLocationProposed.x, filmLocationProposed.y) +=
                                            LProposed *  (accept + (sampler.isCurLargeStep() ? 1.0f : 0.0f)) /
                                            (luminance(LProposed) / b + largeStepProb) ;

                                }
                                image(filmLocationCurrent.x, filmLocationCurrent.y) +=
                                        LCurrent*  (1.0f - accept) / ( luminance(LCurrent) / b + largeStepProb);
                            }else{
                                if (accept > 0.0f){
                                    image(filmLocationProposed.x,filmLocationProposed.y) += LProposed * accept / luminance(LProposed);
                                }
                                image(filmLocationCurrent.x,filmLocationCurrent.y) += LCurrent * (1.0f - accept) / luminance(LCurrent);
                            }
                        }


                        u = randf();
                        if (u < accept) {
                            filmLocationCurrent = filmLocationProposed;
                            LCurrent = LProposed;
                            sampler.accept();
                        } else {
                            sampler.reject();
                        }
                    }
                }

                for (auto j : range(camera->resolution().y)){
                    for (auto i: range(camera->resolution().x))
                        image(i, j) *= ((useMis ? 1.0f : b) / scene.getImageSamples());
                }

                return image;
        }


        virtual bool isMLTIntegrator()
        override{
                return true;
        }

        std::shared_ptr<Integrator> internal_integrator;

    private:
        int sampleFromDiscreteDistribution(const std::vector<float> &cdf) {
            auto size = cdf.size();

            float u = randf();

            auto low = std::lower_bound(cdf.begin(), cdf.end(), u);
            int idx = low - cdf.begin();
            return idx;
        }

        bool isInvalid(float x){
            return std::isnan(x) || std::isinf(x) || x <= 0.0001f ;
        }


        float sigma;
        float largeStepProb;
        int nSampler; // number of sampler used in MCMC iteration
};




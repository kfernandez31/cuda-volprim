#pragma once

#include "vec.h"

#include <optional>

class Material {
public:
    struct ScatterRecord {
        glm::vec3 attenuation;
        Ray ray;
    };


    glm::vec3 color;
    // glm::vec3 ambient = glm::vec3(0.0);
    // glm::vec3 diffuse = glm::vec3(1.0);
    // glm::vec3 specular = glm::vec3(0.0);
    // float shininess = 0.0;

    virtual std::optional<ScatterRecord> scatter(const Ray& r_in, const HitRecord& rec) const {
        return {};
    };
};

class Lambertian : public Material {
public:
    Lambertian(const glm::vec3& albedo) : albedo(albedo) {}

    std::optional<ScatterRecord> scatter(const Ray& r_in, const HitRecord& rec) const override {
        // glm::vec3 scatter_dir;
        // do {
        //     scatter_dir = rec.normal + random_unit_vector();
        // } while (near_zero(scatter_dir)); // Catch degenerate scatter direction

        auto scatter_dir = rec.normal + random_unit_vector();
        if (near_zero(scatter_dir))
            scatter_dir = rec.normal;

        return ScatterRecord {
            .attenuation = albedo,
            .ray = Ray(rec.p, scatter_dir),
        };
    }

private:
    glm::vec3 albedo;
};

class Metal : public Material {
public:
    Metal(const glm::vec3& albedo) : albedo(albedo) {}

    std::optional<ScatterRecord> scatter(const Ray& r_in, const HitRecord& rec) const override {
        glm::vec3 reflected = glm::reflect(r_in.direction, rec.normal);
        reflected = glm::normalize(reflected) + (fuzz * random_unit_vector());

        if (glm::dot(scattered.direction(), rec.normal) > 0) {
            return ScatterRecord {
                .attenuation = albedo,
                .ray = Ray(rec.p, reflected),
            };
        }
    }
private:
    glm::vec3 albedo;
    float fuzz;
};

class Dielectric : public material {
  public:
    Dielectric(float refraction_index) : refraction_index(refraction_index) {}

    std::optional<ScatterRecord> scatter(const Ray& r_in, const HitRecord& rec) const override {
        float ri = rec.front_face ? (1.0 / refraction_index) : refraction_index;
        // float ri = glm::pow(refraction_index, -1 * !rec.front_face);

        float cos_theta = glm::min(glm::dot(-r_in.direction(), rec.normal), 1.0);
        float sin_theta = glm::sqrt(1.0 - glm::pow2(cos_theta));

        bool cannot_refract = ri * sin_theta > 1.0;

        return ScatterRecord {
            .attenuation = glm::vec3(1.0),
            .ray = Ray(rec.p, cannot_refract
                ? glm::reflect(r_in.direction(), rec.normal)
                : glm::refract(r_in.direction(), rec.normal, ri)
            ),
        };
    }

  private:
    // Refractive index in vacuum or air, or the ratio of the material's refractive index over
    // the refractive index of the enclosing media
    float refraction_index;
};

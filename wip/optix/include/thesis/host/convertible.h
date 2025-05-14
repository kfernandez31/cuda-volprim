#pragma once

namespace thesis {
namespace host {

template <typename DeviceType>
class Convertible {
   public:
    virtual ~Convertible() = default;

    [[nodiscard]] virtual DeviceType toDevice() const noexcept = 0;
};

}  // namespace host
}  // namespace thesis

#pragma once

namespace thesis::host::params {

template <typename DeviceType>
class Convertible {
   public:
    virtual ~Convertible() = default;

    [[nodiscard]] virtual DeviceType toDevice() const noexcept = 0;
};

}  // namespace thesis::host::params

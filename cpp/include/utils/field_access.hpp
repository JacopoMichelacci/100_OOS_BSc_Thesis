#pragma once

template <typename Tin, typename Tfield = double>
constexpr Tfield Tin::* default_close_field() {
    if constexpr (requires { static_cast<Tfield Tin::*>(&Tin::close); }) {
        return &Tin::close;
    } else {
        return nullptr;
    }
}

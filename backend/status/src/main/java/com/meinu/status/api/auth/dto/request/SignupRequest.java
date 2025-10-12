package com.meinu.status.api.auth.dto.request;

import jakarta.validation.constraints.Email;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Size;

public record SignupRequest(
        @Email @NotBlank String email,
        @NotBlank @Size(min = 6, max = 64) String password,
        @NotBlank @Size(min = 1, max = 50) String nickname) {
}

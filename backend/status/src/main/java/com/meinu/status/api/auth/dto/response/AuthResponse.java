package com.meinu.status.api.auth.dto.response;

public record AuthResponse(Long memberId, String email, String nickname, String accessToken,
        long accessTokenExpiresIn) {
}

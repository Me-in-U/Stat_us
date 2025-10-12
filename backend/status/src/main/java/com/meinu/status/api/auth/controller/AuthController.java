package com.meinu.status.api.auth.controller;

import com.meinu.status.api.auth.dto.request.LoginRequest;
import com.meinu.status.api.auth.dto.request.SignupRequest;
import com.meinu.status.api.auth.dto.response.AuthResponse;
import com.meinu.status.api.member.entity.Member;
import com.meinu.status.api.member.service.MemberService;
import com.meinu.status.global.common.base.BaseException;
import com.meinu.status.global.common.base.BaseResponse;
import com.meinu.status.global.common.base.BaseResponseStatus;
import com.meinu.status.global.config.security.JwtProperties;
import com.meinu.status.global.config.security.JwtService;
import jakarta.servlet.http.Cookie;
import jakarta.servlet.http.HttpServletResponse;
import jakarta.validation.Valid;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.http.ResponseEntity;
import org.springframework.util.StringUtils;
import org.springframework.web.bind.annotation.*;

import java.time.Duration;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

@RestController
@RequestMapping("/api/auth")
public class AuthController {
    private final MemberService memberService;
    private final JwtService jwtService;
    private final JwtProperties jwtProps;
    private final StringRedisTemplate redis;

    public AuthController(MemberService memberService, JwtService jwtService, JwtProperties jwtProps,
            StringRedisTemplate redis) {
        this.memberService = memberService;
        this.jwtService = jwtService;
        this.jwtProps = jwtProps;
        this.redis = redis;
    }

    @PostMapping("/signup")
    public ResponseEntity<BaseResponse<AuthResponse>> signup(@Valid @RequestBody SignupRequest req,
            HttpServletResponse res) {
        Member m = memberService.signup(req.email(), req.password(), req.nickname());
        return issueTokensAndRespond(m, res);
    }

    @PostMapping("/login")
    public ResponseEntity<BaseResponse<AuthResponse>> login(@Valid @RequestBody LoginRequest req,
            HttpServletResponse res) {
        Member m = memberService.getByEmail(req.email());
        if (!memberService.matches(req.password(), m.getPasswordHash())) {
            throw new BaseException(BaseResponseStatus.INVALID_CREDENTIALS);
        }
        return issueTokensAndRespond(m, res);
    }

    @PostMapping("/refresh")
    public ResponseEntity<BaseResponse<AuthResponse>> refresh(
            @CookieValue(name = "refreshToken", required = false) String refreshToken, HttpServletResponse res) {
        if (!StringUtils.hasText(refreshToken)) {
            throw new BaseException(BaseResponseStatus.REFRESH_TOKEN_NOT_FOUND);
        }
        var claims = jwtService.parse(refreshToken);
        String email = claims.getSubject();
        String key = refreshKey(email);
        String stored = redis.opsForValue().get(key);
        if (!Objects.equals(stored, refreshToken)) {
            throw new BaseException(BaseResponseStatus.INVALID_TOKEN);
        }
        Member m = memberService.getByEmail(email);
        return issueTokensAndRespond(m, res);
    }

    @PostMapping("/logout")
    public ResponseEntity<BaseResponse<Void>> logout(
            @CookieValue(name = "refreshToken", required = false) String refreshToken, HttpServletResponse res) {
        if (StringUtils.hasText(refreshToken)) {
            try {
                var claims = jwtService.parse(refreshToken);
                String email = claims.getSubject();
                redis.delete(refreshKey(email));
            } catch (Exception ignored) {
                // ignore parse errors during logout; cookie will be cleared regardless
            }
        }
        Cookie cookie = new Cookie("refreshToken", "");
        cookie.setHttpOnly(true);
        cookie.setSecure(false);
        cookie.setPath("/");
        cookie.setMaxAge(0);
        res.addCookie(cookie);
        Cookie accessClear = new Cookie("accessToken", "");
        accessClear.setHttpOnly(true);
        accessClear.setSecure(false);
        accessClear.setPath("/");
        accessClear.setMaxAge(0);
        res.addCookie(accessClear);
        return ResponseEntity.ok(BaseResponse.of(BaseResponseStatus.SUCCESS));
    }

    private ResponseEntity<BaseResponse<AuthResponse>> issueTokensAndRespond(Member m, HttpServletResponse res) {
        Map<String, Object> claims = new HashMap<>();
        claims.put("roles", m.getRoles());
        String access = jwtService.generateAccessToken(m.getEmail(), claims);
        String refresh = jwtService.generateRefreshToken(m.getEmail());
        // store refresh in Redis
        redis.opsForValue().set(refreshKey(m.getEmail()), refresh, Duration.ofSeconds(jwtProps.getRefreshExpSeconds()));
        // set cookie
        Cookie cookie = new Cookie("refreshToken", refresh);
        cookie.setHttpOnly(true);
        cookie.setSecure(false);
        cookie.setPath("/");
        cookie.setMaxAge((int) jwtProps.getRefreshExpSeconds());
        res.addCookie(cookie);
        // set access token cookie (for SSE/EventSource without headers)
        Cookie accessCookie = new Cookie("accessToken", access);
        accessCookie.setHttpOnly(true);
        accessCookie.setSecure(false);
        accessCookie.setPath("/");
        accessCookie.setMaxAge((int) jwtProps.getAccessExpSeconds());
        res.addCookie(accessCookie);
        AuthResponse body = new AuthResponse(m.getId(), m.getEmail(), m.getNickname(), access,
                jwtProps.getAccessExpSeconds());
        return ResponseEntity.ok(BaseResponse.success(body));
    }

    private String refreshKey(String email) {
        return "refresh:" + email;
    }
}

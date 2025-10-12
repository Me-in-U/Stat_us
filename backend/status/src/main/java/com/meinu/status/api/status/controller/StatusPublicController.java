package com.meinu.status.api.status.controller;

import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.meinu.status.api.member.entity.Member;
import com.meinu.status.api.member.repository.MemberRepository;
import com.meinu.status.global.common.base.BaseResponse;
import com.meinu.status.global.common.base.BaseResponseStatus;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.util.StringUtils;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.Collections;
import java.util.Map;

@RestController
@RequestMapping("/api/status")
public class StatusPublicController {
    private final StringRedisTemplate redis;
    private final ObjectMapper objectMapper;
    private final MemberRepository memberRepository;

    public StatusPublicController(StringRedisTemplate redis, ObjectMapper objectMapper,
            MemberRepository memberRepository) {
        this.redis = redis;
        this.objectMapper = objectMapper;
        this.memberRepository = memberRepository;
    }

    /**
     * Latest status by API key for device clients (e.g., ESP32). Secured by
     * x-api-key header.
     */
    @GetMapping("/latest/by-key")
    public ResponseEntity<BaseResponse<Map<String, Object>>> latestByKey(
            @RequestHeader(value = "x-api-key", required = false) String apiKey) {
        if (!StringUtils.hasText(apiKey)) {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED)
                    .body(BaseResponse.of(BaseResponseStatus.API_KEY_REQUIRED));
        }
        Member member = memberRepository.findByApiKey(apiKey).orElse(null);
        if (member == null) {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED)
                    .body(BaseResponse.of(BaseResponseStatus.API_KEY_INVALID));
        }
        String key = "status:latest:" + member.getId();
        String json = redis.opsForValue().get(key);
        if (!StringUtils.hasText(json)) {
            return ResponseEntity.ok(BaseResponse.success(Collections.emptyMap()));
        }
        try {
            Map<String, Object> payload = objectMapper.readValue(json, new TypeReference<>() {
            });
            return ResponseEntity.ok(BaseResponse.success(payload));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR)
                    .body(BaseResponse.of(BaseResponseStatus.SERVER_ERROR));
        }
    }
}

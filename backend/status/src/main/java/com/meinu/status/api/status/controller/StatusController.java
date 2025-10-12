package com.meinu.status.api.status.controller;

import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.meinu.status.api.member.entity.Member;
import com.meinu.status.api.member.service.MemberService;
import com.meinu.status.global.common.base.BaseResponse;
import com.meinu.status.global.common.base.BaseResponseStatus;
import org.springframework.http.HttpStatus;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.http.ResponseEntity;
import org.springframework.security.core.Authentication;
import org.springframework.util.StringUtils;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.Collections;
import java.util.Map;

@RestController
@RequestMapping("/api/status")
public class StatusController {
    private final StringRedisTemplate redis;
    private final ObjectMapper objectMapper;
    private final MemberService memberService;

    public StatusController(StringRedisTemplate redis, ObjectMapper objectMapper, MemberService memberService) {
        this.redis = redis;
        this.objectMapper = objectMapper;
        this.memberService = memberService;
    }

    @GetMapping("/latest")
    public ResponseEntity<BaseResponse<Map<String, Object>>> latest(Authentication auth) {
        Member me = memberService.getByEmail(auth.getName());
        String key = "status:latest:" + me.getId();
        String json = redis.opsForValue().get(key);
        if (!StringUtils.hasText(json)) {
            return ResponseEntity.ok(BaseResponse.success(Collections.emptyMap()));
        }
        try {
            Map<String, Object> payload = objectMapper.readValue(json, new TypeReference<>() {
            });
            return ResponseEntity.ok(BaseResponse.success(payload));
        } catch (Exception e) {
            // JSON 파싱 실패 시 서버 오류로 반환
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR)
                    .body(BaseResponse.of(BaseResponseStatus.SERVER_ERROR));
        }
    }
}

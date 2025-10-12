package com.meinu.status.api.member.controller;

import com.meinu.status.api.member.entity.Member;
import com.meinu.status.api.member.service.MemberService;
import com.meinu.status.global.common.base.BaseResponse;
import org.springframework.http.ResponseEntity;
import org.springframework.security.core.Authentication;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api/member")
public class MemberController {
    private final MemberService memberService;

    public MemberController(MemberService memberService) {
        this.memberService = memberService;
    }

    @GetMapping("/me")
    public ResponseEntity<BaseResponse<MemberProfileResponse>> me(Authentication auth) {
        Member m = memberService.getByEmail(auth.getName());
        return ResponseEntity.ok(BaseResponse
                .success(new MemberProfileResponse(m.getId(), m.getEmail(), m.getNickname(), m.getApiKey())));
    }

    @PostMapping("/api-key/issue")
    public ResponseEntity<BaseResponse<ApiKeyResponse>> issueKey(Authentication auth) {
        Member m = memberService.getByEmail(auth.getName());
        String newKey = memberService.issueNewApiKey(m);
        return ResponseEntity.ok(BaseResponse.success(new ApiKeyResponse(newKey)));
    }

    public record MemberProfileResponse(Long id, String email, String nickname, String apiKey) {
    }

    public record ApiKeyResponse(String apiKey) {
    }
}

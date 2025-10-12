package com.meinu.status.global.config.security;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.meinu.status.global.common.base.BaseResponse;
import com.meinu.status.global.common.base.BaseResponseStatus;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.lang.NonNull;
import org.springframework.http.HttpMethod;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.http.HttpStatus;
import org.springframework.security.authentication.UsernamePasswordAuthenticationToken;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.http.SessionCreationPolicy;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.security.web.authentication.UsernamePasswordAuthenticationFilter;
import org.springframework.util.StringUtils;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;
import java.util.List;

@Configuration
public class SecurityConfig {

    private final JwtService jwtService;

    public SecurityConfig(JwtService jwtService) {
        this.jwtService = jwtService;
    }

    @Bean
    public SecurityFilterChain filterChain(HttpSecurity http) throws Exception {
        http
                .csrf(csrf -> csrf.disable())
                // Enable CORS using the CorsConfigurationSource bean
                .cors(cors -> {
                })
                .sessionManagement(sm -> sm.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
                .authorizeHttpRequests(auth -> auth
                        .requestMatchers(HttpMethod.OPTIONS, "/**").permitAll()
                        .requestMatchers("/api/auth/**").permitAll()
                        .requestMatchers("/api/status/latest/by-key").permitAll()
                        .requestMatchers("/api/status/stream").authenticated()
                        .requestMatchers("/api/ingest/vscode").permitAll() // secured by x-api-key
                        .anyRequest().authenticated())
                .addFilterBefore(new JwtAuthFilter(jwtService), UsernamePasswordAuthenticationFilter.class)
                .exceptionHandling(ex -> ex.authenticationEntryPoint((req, res, ex2) -> {
                    res.setStatus(HttpStatus.UNAUTHORIZED.value());
                    res.setContentType("application/json");
                    new ObjectMapper().writeValue(res.getWriter(), BaseResponse.of(BaseResponseStatus.UNAUTHORIZED));
                }));
        return http.build();
    }

    static class JwtAuthFilter extends OncePerRequestFilter {
        private final JwtService jwtService;
        private static final Logger log = LoggerFactory.getLogger(JwtAuthFilter.class);

        JwtAuthFilter(JwtService jwtService) {
            this.jwtService = jwtService;
        }

        @Override
        protected void doFilterInternal(@NonNull HttpServletRequest request, @NonNull HttpServletResponse response,
                @NonNull FilterChain filterChain) throws ServletException, IOException {
            String token = resolveToken(request);
            authenticateIfValid(token, request);
            filterChain.doFilter(request, response);
        }

        private String resolveToken(HttpServletRequest request) {
            String auth = request.getHeader("Authorization");
            if (StringUtils.hasText(auth) && auth.startsWith("Bearer ")) {
                if (log.isDebugEnabled()) {
                    log.debug("JwtAuthFilter: Bearer token found in header for path={} origin={}",
                            request.getRequestURI(), request.getHeader("Origin"));
                }
                return auth.substring(7);
            }
            if (request.getCookies() != null) {
                for (var c : request.getCookies()) {
                    if ("accessToken".equals(c.getName()) && StringUtils.hasText(c.getValue())) {
                        if (log.isDebugEnabled()) {
                            log.debug("JwtAuthFilter: accessToken cookie used for path={} origin={}",
                                    request.getRequestURI(), request.getHeader("Origin"));
                        }
                        return c.getValue();
                    }
                }
            }
            return null;
        }

        private void authenticateIfValid(String token, HttpServletRequest request) {
            if (!StringUtils.hasText(token))
                return;
            try {
                var claims = jwtService.parse(token);
                String sub = claims.getSubject();
                @SuppressWarnings("unchecked")
                var roles = (List<String>) claims.getOrDefault("roles", List.of("ROLE_USER"));
                var authorities = roles.stream().map(SimpleGrantedAuthority::new).toList();
                var authToken = new UsernamePasswordAuthenticationToken(sub, null, authorities);
                SecurityContextHolder.getContext().setAuthentication(authToken);
                if (log.isDebugEnabled()) {
                    log.debug("JwtAuthFilter: authenticated subject={} roles={} path={}", sub, roles,
                            request.getRequestURI());
                }
            } catch (Exception ignored) {
                if (log.isDebugEnabled()) {
                    log.debug("JwtAuthFilter: token parse failed for path={}", request.getRequestURI());
                }
            }
        }
    }
}

package com.meinu.status.global.config;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.crypto.bcrypt.BCryptPasswordEncoder;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.lang.NonNull;
import org.springframework.web.servlet.config.annotation.CorsRegistry;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;

@Configuration
public class WebConfig implements WebMvcConfigurer {

    @Value("${app.cors.allowed-origins:http://localhost:5173}")
    private String allowedOrigins;

    // Header constants to avoid duplicated literals and satisfy linters
    private static final String HDR_AUTHORIZATION = "Authorization";
    private static final String HDR_CONTENT_TYPE = "Content-Type";
    private static final String HDR_X_API_KEY = "x-api-key";
    private static final String HDR_X_REQUESTED_WITH = "X-Requested-With";
    private static final String HDR_ACCEPT = "Accept";
    private static final String HDR_SET_COOKIE = "Set-Cookie";
    private static final String HDR_LAST_EVENT_ID = "Last-Event-ID";

    private static final String[] COMMON_ALLOWED_HEADERS = new String[] {
            HDR_AUTHORIZATION, HDR_CONTENT_TYPE, HDR_X_API_KEY, HDR_X_REQUESTED_WITH, HDR_ACCEPT
    };

    private static final String[] SSE_ALLOWED_HEADERS = new String[] {
            HDR_AUTHORIZATION, HDR_CONTENT_TYPE, HDR_X_API_KEY, HDR_X_REQUESTED_WITH, HDR_ACCEPT, HDR_LAST_EVENT_ID
    };

    private static final String[] COMMON_EXPOSED_HEADERS = new String[] {
            HDR_AUTHORIZATION, HDR_SET_COOKIE
    };

    @Override
    public void addCorsMappings(@NonNull CorsRegistry registry) {
        String[] origins = allowedOrigins.split(",");
        registry.addMapping("/**")
                .allowedOrigins(origins)
                .allowedMethods("GET", "POST", "PUT", "PATCH", "DELETE", "OPTIONS")
                .allowedHeaders(COMMON_ALLOWED_HEADERS)
                .exposedHeaders(COMMON_EXPOSED_HEADERS)
                .allowCredentials(true)
                .maxAge(3600);
        // Explicit mapping for SSE endpoint (may help some proxies)
        registry.addMapping("/api/status/stream")
                .allowedOrigins(origins)
                .allowedMethods("GET", "OPTIONS")
                .allowedHeaders(SSE_ALLOWED_HEADERS)
                .exposedHeaders(COMMON_EXPOSED_HEADERS)
                .allowCredentials(true)
                .maxAge(3600);
    }

    @Bean
    public PasswordEncoder passwordEncoder() {
        return new BCryptPasswordEncoder();
    }
}

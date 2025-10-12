package com.meinu.status.global.common.dto;

import java.util.List;

public record PageResponse<T>(List<T> content, long totalElements, int totalPages, int page, int size) {
}

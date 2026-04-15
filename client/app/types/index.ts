export type Media = {
    id: number;
    title: string;
    file_path: string;
    media_type: string;
    duration_secs: number; 
    file_size: number;
    width: number;
    height: number;
    video_codec: string;
    audio_codec: string;
    thumbnail_path: string;
    added_at: Date;
};